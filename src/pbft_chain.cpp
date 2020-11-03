#include "pbft_chain.hpp"

#include "pbft_manager.hpp"

namespace taraxa {

PbftBlock::PbftBlock(bytes const& b) : PbftBlock(dev::RLP(b)) {}

PbftBlock::PbftBlock(dev::RLP const& r) {
  dev::RLP const rlp(r);
  if (!rlp.isList()) throw std::invalid_argument("PBFT RLP must be a list");
  prev_block_hash_ = rlp[0].toHash<blk_hash_t>();
  dag_block_hash_as_pivot_ = rlp[1].toHash<blk_hash_t>();
  period_ = rlp[2].toInt<uint64_t>();
  timestamp_ = rlp[3].toInt<uint64_t>();
  signature_ = rlp[4].toHash<sig_t>();
  calculateHash_();
}

PbftBlockCert::PbftBlockCert(bytes const& all_rlp)
    : PbftBlockCert(dev::RLP(all_rlp)) {}

PbftBlock::PbftBlock(blk_hash_t const& prev_blk_hash,
                     blk_hash_t const& dag_blk_hash_as_pivot, uint64_t period,
                     addr_t const& beneficiary, secret_t const& sk)
    : prev_block_hash_(prev_blk_hash),
      dag_block_hash_as_pivot_(dag_blk_hash_as_pivot),
      period_(period),
      beneficiary_(beneficiary) {
  timestamp_ = dev::utcTime();
  signature_ = dev::sign(sk, sha3(false));
  calculateHash_();
}

PbftBlock::PbftBlock(std::string const& str) {
  Json::Value doc;
  Json::Reader reader;
  reader.parse(str, doc);
  block_hash_ = blk_hash_t(doc["block_hash"].asString());
  prev_block_hash_ = blk_hash_t(doc["prev_block_hash"].asString());
  dag_block_hash_as_pivot_ =
      blk_hash_t(doc["dag_block_hash_as_pivot"].asString());
  period_ = doc["period"].asUInt64();
  timestamp_ = doc["timestamp"].asUInt64();
  signature_ = sig_t(doc["signature"].asString());
  beneficiary_ = addr_t(doc["beneficiary"].asString());
}

Json::Value PbftBlock::toJson(PbftBlock const& b,
                              std::vector<blk_hash_t> const& dag_blks) {
  auto ret = b.getJson();
  // Legacy schema
  auto& schedule_json = ret["schedule"] = Json::Value(Json::objectValue);
  auto& dag_blks_json = schedule_json["dag_blocks_order"] =
      Json::Value(Json::arrayValue);
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
  beneficiary_ =
      dev::right160(dev::sha3(dev::bytesConstRef(p.data(), sizeof(p))));
}

blk_hash_t PbftBlock::sha3(bool include_sig) const {
  return dev::sha3(rlp(include_sig));
}

std::string PbftBlock::getJsonStr() const { return getJson().toStyledString(); }

Json::Value PbftBlock::getJson() const {
  Json::Value json;
  json["prev_block_hash"] = prev_block_hash_.toString();
  json["dag_block_hash_as_pivot"] = dag_block_hash_as_pivot_.toString();
  json["period"] = (Json::Value::UInt64)period_;
  json["timestamp"] = (Json::Value::UInt64)timestamp_;
  json["block_hash"] = block_hash_.toString();
  json["signature"] = signature_.toString();
  json["beneficiary"] = beneficiary_.toString();
  return json;
}

// Using to setup PBFT block hash
void PbftBlock::streamRLP(dev::RLPStream& strm, bool include_sig) const {
  strm.appendList(include_sig ? 5 : 4);
  strm << prev_block_hash_;
  strm << dag_block_hash_as_pivot_;
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

PbftBlockCert::PbftBlockCert(PbftBlock const& pbft_blk,
                             std::vector<Vote> const& cert_votes)
    : pbft_blk(new PbftBlock(pbft_blk)), cert_votes(cert_votes) {}

PbftBlockCert::PbftBlockCert(dev::RLP const& rlp) {
  pbft_blk.reset(new PbftBlock(rlp[0]));
  for (auto const& el : rlp[1]) {
    cert_votes.emplace_back(el);
  }
}

bytes PbftBlockCert::rlp() const {
  RLPStream s(2);
  s.appendRaw(pbft_blk->rlp(true));
  s.appendList(cert_votes.size());
  for (auto const& v : cert_votes) {
    s.appendRaw(v.rlp());
  }
  return s.out();
}

void PbftBlockCert::encode_raw(RLPStream& rlp, PbftBlock const& pbft_blk,
                               dev::bytesConstRef votes_raw) {
  rlp.appendList(2).appendRaw(pbft_blk.rlp(true)).appendRaw(votes_raw);
}

std::ostream& operator<<(std::ostream& strm, PbftBlockCert const& b) {
  strm << "[PbftBlockCert] : " << b.pbft_blk << " , num of votes "
       << b.cert_votes.size() << std::endl;
  return strm;
}

PbftChain::PbftChain(std::string const& dag_genesis_hash, addr_t node_addr,
                     std::shared_ptr<DbStorage> db)
    : head_hash_(blk_hash_t(0)),
      size_(0),
      last_pbft_block_hash_(blk_hash_t(0)),
      dag_genesis_hash_(blk_hash_t(dag_genesis_hash)),
      db_(db) {
  LOG_OBJECTS_CREATE("PBFT_CHAIN");
  // Get PBFT head from DB
  auto pbft_head_str = db_->getPbftHead(head_hash_);
  if (pbft_head_str.empty()) {
    // Store PBFT HEAD to db
    db_->savePbftHead(head_hash_, getJsonStr());
  } else {
    Json::Value doc;
    Json::Reader reader;
    reader.parse(pbft_head_str, doc);
    auto head_hash = blk_hash_t(doc["head_hash"].asString());
    auto size = doc["size"].asUInt64();
    auto last_pbft_block_hash =
        blk_hash_t(doc["last_pbft_block_hash"].asString());
    setPbftChainHead(head_hash, size, last_pbft_block_hash);
  }
}

void PbftChain::cleanupUnverifiedPbftBlocks(
    taraxa::PbftBlock const& pbft_block) {
  blk_hash_t prev_block_hash = pbft_block.getPrevBlockHash();
  upgradableLock_ lock(unverified_access_);
  if (unverified_blocks_map_.find(prev_block_hash) ==
      unverified_blocks_map_.end()) {
    LOG(log_er_) << "Cannot find the prev PBFT block hash " << prev_block_hash;
    assert(false);
  }
  // cleanup PBFT blocks in unverified_blocks_ table
  upgradeLock_ locked(lock);
  for (blk_hash_t const& block_hash : unverified_blocks_map_[prev_block_hash]) {
    unverified_blocks_.erase(block_hash);
  }
  // cleanup PBFT blocks hash in unverified_blocks_map_ table
  unverified_blocks_map_.erase(prev_block_hash);
}

blk_hash_t PbftChain::getHeadHash() const {
  sharedLock_ lock(chain_head_access_);
  return head_hash_;
}

uint64_t PbftChain::getPbftChainSize() const {
  sharedLock_ lock(chain_head_access_);
  return size_;
}

blk_hash_t PbftChain::getLastPbftBlockHash() const {
  sharedLock_ lock(chain_head_access_);
  return last_pbft_block_hash_;
}

void PbftChain::setLastPbftBlockHash(
    const taraxa::blk_hash_t& last_pbft_block_hash) {
  uniqueLock_ lock(chain_head_access_);
  last_pbft_block_hash_ = last_pbft_block_hash;
}

void PbftChain::setPbftChainHead(
    const taraxa::blk_hash_t& head_hash, const uint64_t size,
    const taraxa::blk_hash_t& last_pbft_block_hash) {
  uniqueLock_ lock(chain_head_access_);
  head_hash_ = head_hash;
  size_ = size;
  last_pbft_block_hash_ = last_pbft_block_hash;
}

bool PbftChain::findPbftBlockInChain(
    taraxa::blk_hash_t const& pbft_block_hash) const {
  if (!db_) {
    LOG(log_er_) << "Pbft chain DB unavailable in findPbftBlockInChain!";
    return false;
  }

  assert(db_);
  return db_->pbftBlockInDb(pbft_block_hash);
}

bool PbftChain::findUnverifiedPbftBlock(
    taraxa::blk_hash_t const& pbft_block_hash) const {
  sharedLock_ lock(unverified_access_);
  return unverified_blocks_.find(pbft_block_hash) != unverified_blocks_.end();
}

bool PbftChain::findPbftBlockInSyncedSet(
    taraxa::blk_hash_t const& pbft_block_hash) const {
  return pbft_synced_set_.find(pbft_block_hash) != pbft_synced_set_.end();
}

PbftBlock PbftChain::getPbftBlockInChain(
    const taraxa::blk_hash_t& pbft_block_hash) {
  auto pbft_block = db_->getPbftBlock(pbft_block_hash);
  if (pbft_block == nullptr) {
    LOG(log_er_) << "Cannot find PBFT block hash " << pbft_block_hash
                 << " in DB";
    assert(false);
  }
  return *pbft_block;
}

std::shared_ptr<PbftBlock> PbftChain::getUnverifiedPbftBlock(
    const taraxa::blk_hash_t& pbft_block_hash) {
  if (findUnverifiedPbftBlock(pbft_block_hash)) {
    sharedLock_ lock(unverified_access_);
    return unverified_blocks_[pbft_block_hash];
  }
  return {};
}

std::vector<PbftBlock> PbftChain::getPbftBlocks(size_t period,
                                                size_t count) const {
  // TODO batch-get
  std::vector<PbftBlock> result;
  for (auto i = period; i < period + count; i++) {
    auto pbft_block_hash = db_->getPeriodPbftBlock(i);
    if (pbft_block_hash == nullptr) {
      LOG(log_er_) << "PBFT block period " << i
                   << " is not exist in blocks order DB.";
      break;
    }
    auto pbft_block = db_->getPbftBlock(*pbft_block_hash);
    if (pbft_block == nullptr) {
      LOG(log_er_) << "Cannot find PBFT block hash " << pbft_block_hash
                   << " in PBFT chain DB.";
      break;
    }
    if (pbft_block->getPeriod() != i) {
      LOG(log_er_) << "DB corrupted - PBFT block hash " << pbft_block_hash
                   << "has different period " << pbft_block->getPeriod()
                   << "in block data then in block order db: " << i;
      assert(false);
    }
    result.push_back(*pbft_block);
  }
  return result;
}

std::vector<std::string> PbftChain::getPbftBlocksStr(size_t period,
                                                     size_t count,
                                                     bool hash) const {
  std::vector<std::string> result;
  for (auto i = period; i < period + count; i++) {
    auto pbft_block_hash = db_->getPeriodPbftBlock(i);
    if (pbft_block_hash == nullptr) {
      LOG(log_er_) << "PBFT block period " << i
                   << " is not exist in blocks order DB.";
      break;
    }
    auto pbft_block = db_->getPbftBlock(*pbft_block_hash);
    if (pbft_block == nullptr) {
      LOG(log_er_) << "Cannot find PBFT block hash "
                   << pbft_block_hash->toString() << " in PBFT chain DB.";
      break;
    }
    if (hash)
      result.push_back(pbft_block_hash->toString());
    else
      result.push_back(pbft_block->getJsonStr());
  }
  return result;
}

void PbftChain::updatePbftChain(blk_hash_t const& pbft_block_hash) {
  pbftSyncedSetInsert_(pbft_block_hash);
  uniqueLock_ lock(chain_head_access_);
  size_++;
  last_pbft_block_hash_ = pbft_block_hash;
}

bool PbftChain::checkPbftBlockValidationFromSyncing(
    taraxa::PbftBlock const& pbft_block) const {
  if (pbftSyncedQueueEmpty()) {
    // The last PBFT block in the chain
    return checkPbftBlockValidation(pbft_block);
  } else {
    // The last PBFT block in the synced queue
    auto last_pbft_block = pbftSyncedQueueBack().pbft_blk;
    if (pbft_block.getPrevBlockHash() != last_pbft_block->getBlockHash()) {
      LOG(log_er_) << "Last PBFT block hash " << last_pbft_block->getBlockHash()
                   << " Invalid PBFT prev block hash "
                   << pbft_block.getPrevBlockHash();
      return false;
    }
    // TODO: Need to verify block signature
    return true;
  }
}

bool PbftChain::checkPbftBlockValidation(
    taraxa::PbftBlock const& pbft_block) const {
  auto last_pbft_block_hash = getLastPbftBlockHash();
  if (pbft_block.getPrevBlockHash() != last_pbft_block_hash) {
    LOG(log_er_) << "PBFT chain last block hash " << last_pbft_block_hash
                 << " Invalid PBFT prev block hash "
                 << pbft_block.getPrevBlockHash();
    return false;
  }
  // TODO: Need to verify block signature
  return true;
}

void PbftChain::pushUnverifiedPbftBlock(
    std::shared_ptr<PbftBlock> const& pbft_block) {
  blk_hash_t block_hash = pbft_block->getBlockHash();
  blk_hash_t prev_block_hash = pbft_block->getPrevBlockHash();
  if (prev_block_hash != getLastPbftBlockHash()) {
    if (findPbftBlockInChain(block_hash)) {
      // The block comes from slow node, drop
      return;
    } else {
      // TODO: The block comes from fast node that should insert.
      //  Or comes from malicious node, need check
    }
  }
  // Store in unverified_blocks_map_ for cleaning later
  insertUnverifiedPbftBlockIntoParentMap_(prev_block_hash, block_hash);
  // Store in unverified_blocks_ table
  uniqueLock_ lock(unverified_access_);
  unverified_blocks_[pbft_block->getBlockHash()] = pbft_block;
  LOG(log_dg_) << "Push unverified block " << block_hash
               << ". Pbft unverified blocks size: "
               << unverified_blocks_.size();
}

std::string PbftChain::getHeadStr() const {
  std::stringstream strm;
  strm << "[PbftChain]" << std::endl;
  strm << "head hash: " << getHeadHash().toString() << std::endl;
  strm << "size: " << getPbftChainSize() << std::endl;
  strm << "last pbft block hash: " << getLastPbftBlockHash().toString()
       << std::endl;
  return strm.str();
}

std::string PbftChain::getJsonStr() const {
  Json::Value json;
  json["head_hash"] = getHeadHash().toString();
  json["size"] = (Json::Value::UInt64)getPbftChainSize();
  json["last_pbft_block_hash"] = getLastPbftBlockHash().toString();
  json["dag_genesis_hash"] = dag_genesis_hash_.toString();
  return json.toStyledString();
}

std::ostream& operator<<(std::ostream& strm, PbftChain const& pbft_chain) {
  strm << pbft_chain.getHeadStr();
  return strm;
}

bool PbftChain::isKnownPbftBlockForSyncing(
    taraxa::blk_hash_t const& pbft_block_hash) const {
  return findPbftBlockInSyncedSet(pbft_block_hash) ||
         findPbftBlockInChain(pbft_block_hash);
}

uint64_t PbftChain::pbftSyncingPeriod() const {
  if (pbft_synced_queue_.empty()) {
    return getPbftChainSize();
  } else {
    return pbftSyncedQueueBack().pbft_blk->getPeriod();
  }
}

size_t PbftChain::pbftSyncedQueueSize() const {
  sharedLock_ lock(sync_access_);
  return pbft_synced_queue_.size();
}

bool PbftChain::pbftSyncedQueueEmpty() const {
  sharedLock_ lock(sync_access_);
  return pbft_synced_queue_.empty();
}

PbftBlockCert PbftChain::pbftSyncedQueueFront() const {
  sharedLock_ lock(sync_access_);
  return pbft_synced_queue_.front();
}

PbftBlockCert PbftChain::pbftSyncedQueueBack() const {
  sharedLock_ lock(sync_access_);
  return pbft_synced_queue_.back();
}

void PbftChain::pbftSyncedQueuePopFront() {
  pbftSyncedSetErase_();
  uniqueLock_ lock(sync_access_);
  pbft_synced_queue_.pop_front();
}

void PbftChain::setSyncedPbftBlockIntoQueue(
    PbftBlockCert const& pbft_block_and_votes) {
  LOG(log_nf_) << "Receive pbft block "
               << pbft_block_and_votes.pbft_blk->getBlockHash()
               << " from peer and push into synced queue";
  uniqueLock_ lock(sync_access_);

  // NOTE: We have already checked that block is being added in
  //       order, in taraxa capability

  pbft_synced_queue_.emplace_back(pbft_block_and_votes);
  pbft_synced_set_.insert(pbft_block_and_votes.pbft_blk->getBlockHash());
}

void PbftChain::clearSyncedPbftBlocks() {
  uniqueLock_ lock(sync_access_);
  pbft_synced_queue_.clear();
  pbft_synced_set_.clear();
}

void PbftChain::pbftSyncedSetInsert_(blk_hash_t const& pbft_block_hash) {
  uniqueLock_ lock(sync_access_);
  pbft_synced_set_.insert(pbft_block_hash);
}

void PbftChain::pbftSyncedSetErase_() {
  auto pbft_block = pbftSyncedQueueFront().pbft_blk;
  uniqueLock_ lock(sync_access_);
  pbft_synced_set_.erase(pbft_block->getBlockHash());
}

void PbftChain::insertUnverifiedPbftBlockIntoParentMap_(
    blk_hash_t const& prev_block_hash, blk_hash_t const& block_hash) {
  upgradableLock_ lock(unverified_access_);
  if (unverified_blocks_map_.find(prev_block_hash) ==
      unverified_blocks_map_.end()) {
    upgradeLock_ locked(lock);
    unverified_blocks_map_[prev_block_hash] = {block_hash};
  } else {
    upgradeLock_ locked(lock);
    unverified_blocks_map_[prev_block_hash].emplace_back(block_hash);
  }
}
}  // namespace taraxa
