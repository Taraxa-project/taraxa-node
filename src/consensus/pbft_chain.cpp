#include "pbft_chain.hpp"

#include <libdevcore/CommonJS.h>

#include "pbft_manager.hpp"
#include "util/jsoncpp.hpp"

using namespace std;

namespace taraxa {

PbftBlock::PbftBlock(bytes const& b) : PbftBlock(dev::RLP(b)) {}

PbftBlock::PbftBlock(dev::RLP const& r) {
  dev::RLP const rlp(r);
  if (!rlp.isList()) throw std::invalid_argument("PBFT RLP must be a list");
  auto it = rlp.begin();

  prev_block_hash_ = (*it++).toHash<blk_hash_t>();
  dag_block_hash_as_pivot_ = (*it++).toHash<blk_hash_t>();
  order_hash_ = (*it++).toHash<blk_hash_t>();
  period_ = (*it++).toInt<uint64_t>();
  timestamp_ = (*it++).toInt<uint64_t>();
  signature_ = (*it++).toHash<sig_t>();
  calculateHash_();
}

PbftBlock::PbftBlock(blk_hash_t const& prev_blk_hash, blk_hash_t const& dag_blk_hash_as_pivot,
                     blk_hash_t const& order_hash, uint64_t period, addr_t const& beneficiary, secret_t const& sk)
    : prev_block_hash_(prev_blk_hash),
      dag_block_hash_as_pivot_(dag_blk_hash_as_pivot),
      order_hash_(order_hash),
      period_(period),
      beneficiary_(beneficiary) {
  timestamp_ = dev::utcTime();
  signature_ = dev::sign(sk, sha3(false));
  calculateHash_();
}

PbftBlock::PbftBlock(std::string const& str) {
  auto doc = util::parse_json(str);
  block_hash_ = blk_hash_t(doc["block_hash"].asString());
  prev_block_hash_ = blk_hash_t(doc["prev_block_hash"].asString());
  dag_block_hash_as_pivot_ = blk_hash_t(doc["dag_block_hash_as_pivot"].asString());
  order_hash_ = blk_hash_t(doc["order_hash"].asString());
  period_ = doc["period"].asUInt64();
  timestamp_ = doc["timestamp"].asUInt64();
  signature_ = sig_t(doc["signature"].asString());
  beneficiary_ = addr_t(doc["beneficiary"].asString());
}

Json::Value PbftBlock::toJson(PbftBlock const& b, std::vector<blk_hash_t> const& dag_blks) {
  auto ret = b.getJson();
  // Legacy schema
  auto& schedule_json = ret["schedule"] = Json::Value(Json::objectValue);
  auto& dag_blks_json = schedule_json["dag_blocks_order"] = Json::Value(Json::arrayValue);
  for (auto const& h : dag_blks) {
    dag_blks_json.append(dev::toJS(h));
  }
  return ret;
}

void PbftBlock::calculateHash_() {
  if (!block_hash_) {
    block_hash_ = dev::sha3(rlp(true));
  } else {
    // Hash sould only be calculated once
    assert(false);
  }
  auto p = dev::recover(signature_, sha3(false));
  assert(p);
  beneficiary_ = dev::right160(dev::sha3(dev::bytesConstRef(p.data(), sizeof(p))));
}

blk_hash_t PbftBlock::sha3(bool include_sig) const { return dev::sha3(rlp(include_sig)); }

std::string PbftBlock::getJsonStr() const { return getJson().toStyledString(); }

Json::Value PbftBlock::getJson() const {
  Json::Value json;
  json["prev_block_hash"] = prev_block_hash_.toString();
  json["dag_block_hash_as_pivot"] = dag_block_hash_as_pivot_.toString();
  json["order_hash"] = order_hash_.toString();
  json["period"] = (Json::Value::UInt64)period_;
  json["timestamp"] = (Json::Value::UInt64)timestamp_;
  json["block_hash"] = block_hash_.toString();
  json["signature"] = signature_.toString();
  json["beneficiary"] = beneficiary_.toString();
  return json;
}

// Using to setup PBFT block hash
void PbftBlock::streamRLP(dev::RLPStream& strm, bool include_sig) const {
  strm.appendList(include_sig ? 6 : 5);
  strm << prev_block_hash_;
  strm << dag_block_hash_as_pivot_;
  strm << order_hash_;
  strm << period_;
  strm << timestamp_;
  if (include_sig) {
    strm << signature_;
  }
}

bytes PbftBlock::rlp(bool include_sig) const {
  RLPStream strm;
  streamRLP(strm, include_sig);
  return strm.out();
}

std::ostream& operator<<(std::ostream& strm, PbftBlock const& pbft_blk) {
  strm << pbft_blk.getJsonStr();
  return strm;
}

PbftChain::PbftChain(blk_hash_t const& dag_genesis_hash, addr_t node_addr, std::shared_ptr<DbStorage> db)
    : head_hash_(blk_hash_t(0)),
      dag_genesis_hash_(dag_genesis_hash),
      size_(0),
      last_pbft_block_hash_(blk_hash_t(0)),
      db_(move(db)) {
  LOG_OBJECTS_CREATE("PBFT_CHAIN");
  // Get PBFT head from DB
  auto pbft_head_str = db_->getPbftHead(head_hash_);
  if (pbft_head_str.empty()) {
    // Store PBFT HEAD to db
    auto head_json_str = getJsonStr();
    db_->savePbftHead(head_hash_, head_json_str);
    LOG(log_nf_) << "Initialize PBFT chain head " << head_json_str;
    return;
  }
  Json::Value doc;
  istringstream(pbft_head_str) >> doc;
  head_hash_ = blk_hash_t(doc["head_hash"].asString());
  size_ = doc["size"].asUInt64();
  last_pbft_block_hash_ = blk_hash_t(doc["last_pbft_block_hash"].asString());
  auto dag_genesis_hash_db = blk_hash_t(doc["dag_genesis_hash"].asString());
  assert(dag_genesis_hash_ == dag_genesis_hash_db);
  LOG(log_nf_) << "Retrieve from DB, PBFT chain head " << getJsonStr();
}

blk_hash_t PbftChain::getHeadHash() const {
  SharedLock lock(chain_head_access_);
  return head_hash_;
}

uint64_t PbftChain::getPbftChainSize() const {
  SharedLock lock(chain_head_access_);
  return size_;
}

blk_hash_t PbftChain::getLastPbftBlockHash() const {
  SharedLock lock(chain_head_access_);
  return last_pbft_block_hash_;
}

bool PbftChain::findPbftBlockInChain(taraxa::blk_hash_t const& pbft_block_hash) {
  return db_->pbftBlockInDb(pbft_block_hash);
}

bool PbftChain::findUnverifiedPbftBlock(taraxa::blk_hash_t const& pbft_block_hash) const {
  SharedLock lock(unverified_access_);
  return unverified_blocks_.find(pbft_block_hash) != unverified_blocks_.end();
}

PbftBlock PbftChain::getPbftBlockInChain(const taraxa::blk_hash_t& pbft_block_hash) {
  auto pbft_block = db_->getPbftBlock(pbft_block_hash);
  if (!pbft_block.has_value()) {
    LOG(log_er_) << "Cannot find PBFT block hash " << pbft_block_hash << " in DB";
    assert(false);
  }
  return pbft_block.value();
}

std::shared_ptr<PbftBlock> PbftChain::getUnverifiedPbftBlock(const taraxa::blk_hash_t& pbft_block_hash) {
  SharedLock lock(unverified_access_);
  auto found_block = unverified_blocks_.find(pbft_block_hash);
  if (found_block == unverified_blocks_.end()) {
    return nullptr;
  }

  return found_block->second;
}

// TODO: should remove, need check
std::vector<std::string> PbftChain::getPbftBlocksStr(size_t period, size_t count, bool hash) const {
  std::vector<std::string> result;
  for (auto i = period; i < period + count; i++) {
    auto pbft_block = db_->getPbftBlock(i);
    if (!pbft_block.has_value()) {
      LOG(log_er_) << "PBFT block period " << i << " does not exist in blocks order DB.";
      break;
    }
    if (hash)
      result.push_back(pbft_block->getBlockHash().toString());
    else
      result.push_back(pbft_block->getJsonStr());
  }
  return result;
}

void PbftChain::updatePbftChain(blk_hash_t const& pbft_block_hash) {
  UniqueLock lock(chain_head_access_);
  size_++;
  last_pbft_block_hash_ = pbft_block_hash;
}

bool PbftChain::checkPbftBlockValidation(taraxa::PbftBlock const& pbft_block) const {
  auto last_pbft_block_hash = getLastPbftBlockHash();
  if (pbft_block.getPrevBlockHash() != last_pbft_block_hash) {
    LOG(log_er_) << "PBFT chain last block hash " << last_pbft_block_hash << " Invalid PBFT prev block hash "
                 << pbft_block.getPrevBlockHash() << " in block " << pbft_block.getBlockHash();
    return false;
  }
  return true;
}

void PbftChain::cleanupUnverifiedPbftBlocks(taraxa::PbftBlock const& pbft_block) {
  blk_hash_t prev_block_hash = pbft_block.getPrevBlockHash();
  UpgradableLock lock(unverified_access_);
  if (unverified_blocks_map_.find(prev_block_hash) == unverified_blocks_map_.end()) {
    LOG(log_er_) << "Cannot find the prev PBFT block hash " << prev_block_hash;
    assert(false);
    return;
  }

  // cleanup PBFT blocks in unverified_blocks_ table
  UpgradeLock locked(lock);
  for (blk_hash_t const& block_hash : unverified_blocks_map_[prev_block_hash]) {
    unverified_blocks_.erase(block_hash);
  }
  // cleanup PBFT blocks hash in unverified_blocks_map_ table
  unverified_blocks_map_.erase(prev_block_hash);
}

bool PbftChain::pushUnverifiedPbftBlock(std::shared_ptr<PbftBlock> const& pbft_block) {
  blk_hash_t block_hash = pbft_block->getBlockHash();
  blk_hash_t prev_block_hash = pbft_block->getPrevBlockHash();
  if (prev_block_hash != getLastPbftBlockHash()) {
    if (findPbftBlockInChain(block_hash)) {
      // The block comes from slow node, drop
      LOG(log_dg_) << "Cannot add the PBFT block " << block_hash << " since it's in chain already";
      return false;
    } else {
      // TODO: The block comes from fast node that should insert.
      //  Or comes from malicious node, need check
    }
  }

  {
    // Store in unverified_blocks_ table
    UniqueLock lock(unverified_access_);
    if (!unverified_blocks_.insert({block_hash, pbft_block}).second) {
      LOG(log_dg_) << "Pbft block " << block_hash.abridged() << " already in unverified queue";
      return false;
    }

    // Store in unverified_blocks_map_ for cleaning later
    unverified_blocks_map_[prev_block_hash].emplace_back(block_hash);
  }

  LOG(log_dg_) << "Push unverified block " << block_hash
               << ". Pbft unverified blocks size: " << unverified_blocks_.size();
  return true;
}

std::string PbftChain::getJsonStr() const {
  Json::Value json;
  SharedLock lock(chain_head_access_);
  json["head_hash"] = head_hash_.toString();
  json["dag_genesis_hash"] = dag_genesis_hash_.toString();
  json["size"] = (Json::Value::UInt64)size_;
  json["last_pbft_block_hash"] = last_pbft_block_hash_.toString();
  return json.toStyledString();
}

std::string PbftChain::getJsonStrForBlock(blk_hash_t const& block_hash) const {
  Json::Value json;
  SharedLock lock(chain_head_access_);
  json["head_hash"] = head_hash_.toString();
  json["dag_genesis_hash"] = dag_genesis_hash_.toString();
  json["size"] = (Json::Value::UInt64)size_ + 1;
  json["last_pbft_block_hash"] = block_hash.toString();
  return json.toStyledString();
}

std::ostream& operator<<(std::ostream& strm, PbftChain const& pbft_chain) {
  strm << pbft_chain.getJsonStr();
  return strm;
}

}  // namespace taraxa
