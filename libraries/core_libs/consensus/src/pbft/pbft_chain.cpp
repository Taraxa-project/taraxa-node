#include "pbft/pbft_chain.hpp"

#include <libdevcore/CommonJS.h>

#include "pbft/pbft_manager.hpp"

using namespace std;

namespace taraxa {

PbftChain::PbftChain(const FullNodeConfig& config, addr_t node_addr, std::shared_ptr<DbStorage> db)
    : kConfig(config), size_(0), non_empty_size_(0), db_(std::move(db)) {
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
  non_empty_size_ = doc["non_empty_size"].asUInt64();
  last_pbft_block_hash_ = blk_hash_t(doc["last_pbft_block_hash"].asString());
  if (kConfig.genesis.state.hardforks.isOnFragariaHardfork(size_)) {
    second_last_pbft_block_hash_ = {doc["second_last_pbft_block_period"].asUInt64(),
                                    blk_hash_t(doc["second_last_pbft_block_hash"].asString())};
  }

  // Retrieve last_non_null_pbft_dag_anchor_hash_ from chain
  if (last_pbft_block_hash_) {
    auto prev_pbft_block = getPbftBlockInChain(last_pbft_block_hash_);
    last_non_null_pbft_dag_anchor_hash_ = prev_pbft_block.getPivotDagBlockHash();
    while (!last_non_null_pbft_dag_anchor_hash_) {
      auto prev_block_hash = prev_pbft_block.getPrevBlockHash();
      if (!prev_block_hash) {
        break;
      }
      prev_pbft_block = getPbftBlockInChain(prev_block_hash);
      last_non_null_pbft_dag_anchor_hash_ = prev_pbft_block.getPivotDagBlockHash();
    }
  }
  LOG(log_nf_) << "Retrieve from DB, PBFT chain head " << getJsonStr();
}

blk_hash_t PbftChain::getHeadHash() const {
  std::shared_lock lock(chain_head_access_);
  return head_hash_;
}

PbftPeriod PbftChain::getPbftChainSize() const {
  std::shared_lock lock(chain_head_access_);
  return size_;
}

PbftPeriod PbftChain::getPbftChainSizeExcludingEmptyPbftBlocks() const {
  std::shared_lock lock(chain_head_access_);
  return non_empty_size_;
}

blk_hash_t PbftChain::getLastPbftBlockHash() const {
  std::shared_lock lock(chain_head_access_);
  return last_pbft_block_hash_;
}

std::pair<PbftPeriod, blk_hash_t> PbftChain::getSecondLastPbftBlockHash() const {
  std::shared_lock lock(chain_head_access_);
  return second_last_pbft_block_hash_;
}

blk_hash_t PbftChain::getLastNonNullPbftBlockAnchor() const {
  std::shared_lock lock(chain_head_access_);
  return last_non_null_pbft_dag_anchor_hash_;
}

bool PbftChain::findPbftBlockInChain(taraxa::blk_hash_t const& pbft_block_hash) {
  return db_->pbftBlockInDb(pbft_block_hash);
}

PbftBlock PbftChain::getPbftBlockInChain(const taraxa::blk_hash_t& pbft_block_hash) {
  auto pbft_block = db_->getPbftBlock(pbft_block_hash);
  if (!pbft_block.has_value()) {
    LOG(log_er_) << "Cannot find PBFT block hash " << pbft_block_hash << " in DB";
    assert(false);
  }
  return *pbft_block;
}

void PbftChain::updatePbftChain(blk_hash_t const& pbft_block_hash, blk_hash_t const& anchor_hash) {
  std::scoped_lock lock(chain_head_access_);
  if (anchor_hash != kNullBlockHash) {
    non_empty_size_++;
    last_non_null_pbft_dag_anchor_hash_ = anchor_hash;
  }

  second_last_pbft_block_hash_ = {size_, last_pbft_block_hash_};
  last_pbft_block_hash_ = pbft_block_hash;
  size_++;
}

bool PbftChain::checkPbftBlockValidation(const std::shared_ptr<PbftBlock>& pbft_block) const {
  if (getPbftChainSize() + 1 != pbft_block->getPeriod()) {
    LOG(log_dg_) << "Pbft validation failed. PBFT chain size " << getPbftChainSize()
                 << ". Pbft block period: " << pbft_block->getPeriod() << " for block " << pbft_block->getBlockHash();
    return false;
  }

  auto last_pbft_block_hash = getLastPbftBlockHash();
  if (pbft_block->getPrevBlockHash() != last_pbft_block_hash) {
    LOG(log_er_) << "PBFT chain last block hash " << last_pbft_block_hash << " Invalid PBFT prev block hash "
                 << pbft_block->getPrevBlockHash() << " in block " << pbft_block->getBlockHash();
    return false;
  }

  return true;
}

std::string PbftChain::getJsonStr() const {
  Json::Value json;
  std::shared_lock lock(chain_head_access_);
  json["head_hash"] = head_hash_.toString();
  json["size"] = static_cast<Json::Value::UInt64>(size_);
  json["non_empty_size"] = static_cast<Json::Value::UInt64>(non_empty_size_);
  json["last_pbft_block_hash"] = last_pbft_block_hash_.toString();
  if (kConfig.genesis.state.hardforks.isOnFragariaHardfork(size_)) {
    json["second_last_pbft_block_period"] = static_cast<Json::Value::UInt64>(second_last_pbft_block_hash_.first);
    json["second_last_pbft_block_hash"] = second_last_pbft_block_hash_.second.toString();
  }

  return json.toStyledString();
}

std::string PbftChain::getJsonStrForBlock(blk_hash_t const& block_hash, bool null_anchor) const {
  Json::Value json;
  std::shared_lock lock(chain_head_access_);
  json["head_hash"] = head_hash_.toString();
  auto size = static_cast<Json::Value::UInt64>(size_) + 1;
  json["size"] = size;
  auto non_empty_size = non_empty_size_;
  if (!null_anchor) {
    non_empty_size++;
  }
  json["non_empty_size"] = (Json::Value::UInt64)non_empty_size;
  json["last_pbft_block_hash"] = block_hash.toString();
  if (kConfig.genesis.state.hardforks.isOnFragariaHardfork(size)) {
    json["second_last_pbft_block_period"] = (Json::Value::UInt64)second_last_pbft_block_hash_.first;
    json["second_last_pbft_block_hash"] = second_last_pbft_block_hash_.second.toString();
  }

  return json.toStyledString();
}

std::ostream& operator<<(std::ostream& strm, PbftChain const& pbft_chain) {
  strm << pbft_chain.getJsonStr();
  return strm;
}

}  // namespace taraxa
