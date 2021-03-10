#include "pbft_chain.hpp"

#include <libdevcore/CommonJS.h>

namespace taraxa {
using namespace std;
using namespace dev;

PbftChain::PbftChain(std::string const& dag_genesis_hash, addr_t node_addr, std::shared_ptr<DB> db)
    : head_hash_(blk_hash_t(0)),
      size_(0),
      last_pbft_block_hash_(blk_hash_t(0)),
      dag_genesis_hash_(blk_hash_t(dag_genesis_hash)),
      db_(move(db)) {
  LOG_OBJECTS_CREATE("PBFT_CHAIN");
  // Get PBFT head from DB
  auto pbft_head_str = db_->getPbftHead(head_hash_);
  if (pbft_head_str.empty()) {
    // Store PBFT HEAD to db
    auto head_json_str = getJsonStr();
    auto b = db_->createWriteBatch();
    b.addPbftHead(head_hash_, head_json_str);
    b.commit();
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

bool PbftChain::findPbftBlockInChain(taraxa::blk_hash_t const& pbft_block_hash) {
  if (!db_) {
    LOG(log_er_) << "Pbft chain DB unavailable in findPbftBlockInChain!";
    return false;
  }
  assert(db_);
  return db_->pbftBlockInDb(pbft_block_hash);
}

bool PbftChain::findUnverifiedPbftBlock(taraxa::blk_hash_t const& pbft_block_hash) const {
  sharedLock_ lock(unverified_access_);
  return unverified_blocks_.find(pbft_block_hash) != unverified_blocks_.end();
}

bool PbftChain::findPbftBlockInSyncedSet(taraxa::blk_hash_t const& pbft_block_hash) const {
  return pbft_synced_set_.find(pbft_block_hash) != pbft_synced_set_.end();
}

PbftBlock PbftChain::getPbftBlockInChain(const taraxa::blk_hash_t& pbft_block_hash) {
  auto pbft_block = db_->getPbftBlock(pbft_block_hash);
  if (pbft_block == nullptr) {
    LOG(log_er_) << "Cannot find PBFT block hash " << pbft_block_hash << " in DB";
    assert(false);
  }
  return *pbft_block;
}

std::shared_ptr<PbftBlock> PbftChain::getUnverifiedPbftBlock(const taraxa::blk_hash_t& pbft_block_hash) {
  if (findUnverifiedPbftBlock(pbft_block_hash)) {
    sharedLock_ lock(unverified_access_);
    return unverified_blocks_[pbft_block_hash];
  }
  return {};
}

std::vector<PbftBlockCert> PbftChain::getPbftBlocks(size_t period, size_t count) {
  std::vector<PbftBlockCert> result;
  for (auto i = period; i < period + count; i++) {
    // Get PBFT block hash by period
    auto pbft_block_hash = db_->getPeriodPbftBlock(i);
    if (!pbft_block_hash) {
      LOG(log_er_) << "PBFT block period " << i << " is not exist in DB period_pbft_block.";
      break;
    }
    // Get PBFT block in DB
    auto pbft_block = db_->getPbftBlock(*pbft_block_hash);
    if (!pbft_block) {
      LOG(log_er_) << "DB corrupted - Cannot find PBFT block hash " << pbft_block_hash
                   << " in PBFT chain DB pbft_blocks.";
      assert(false);
    }
    if (pbft_block->getPeriod() != i) {
      LOG(log_er_) << "DB corrupted - PBFT block hash " << pbft_block_hash << "has different period "
                   << pbft_block->getPeriod() << "in block data then in block order db: " << i;
      assert(false);
    }
    // Get PBFT cert votes in DB
    auto cert_votes_raw = db_->getVotes(*pbft_block_hash);
    vector<Vote> cert_votes;
    for (auto const& cert_vote : RLP(cert_votes_raw)) {
      cert_votes.emplace_back(cert_vote);
    }
    if (cert_votes.empty()) {
      LOG(log_er_) << "Cannot find any cert votes for PBFT block " << pbft_block;
      assert(false);
    }
    PbftBlockCert pbft_block_cert_votes(*pbft_block, cert_votes);
    result.push_back(pbft_block_cert_votes);
  }
  return result;
}

// TODO: should remove, need check
std::vector<std::string> PbftChain::getPbftBlocksStr(size_t period, size_t count, bool hash) const {
  std::vector<std::string> result;
  for (auto i = period; i < period + count; i++) {
    auto pbft_block_hash = db_->getPeriodPbftBlock(i);
    if (pbft_block_hash == nullptr) {
      LOG(log_er_) << "PBFT block period " << i << " is not exist in blocks order DB.";
      break;
    }
    auto pbft_block = db_->getPbftBlock(*pbft_block_hash);
    if (pbft_block == nullptr) {
      LOG(log_er_) << "Cannot find PBFT block hash " << pbft_block_hash->toString() << " in PBFT chain DB.";
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

bool PbftChain::checkPbftBlockValidationFromSyncing(taraxa::PbftBlock const& pbft_block) const {
  if (pbftSyncedQueueEmpty()) {
    // The last PBFT block in the chain
    return checkPbftBlockValidation(pbft_block);
  } else {
    // The last PBFT block in the synced queue
    auto last_pbft_block = pbftSyncedQueueBack().pbft_blk;
    if (pbft_block.getPrevBlockHash() != last_pbft_block->getBlockHash()) {
      LOG(log_er_) << "Last PBFT block hash in synced queue " << last_pbft_block->getBlockHash()
                   << " Invalid PBFT prev block hash " << pbft_block.getPrevBlockHash() << " in block "
                   << pbft_block.getBlockHash();
      return false;
    }
    // TODO: Need to verify block signature
    return true;
  }
}

bool PbftChain::checkPbftBlockValidation(taraxa::PbftBlock const& pbft_block) const {
  auto last_pbft_block_hash = getLastPbftBlockHash();
  if (pbft_block.getPrevBlockHash() != last_pbft_block_hash) {
    LOG(log_er_) << "PBFT chain last block hash " << last_pbft_block_hash << " Invalid PBFT prev block hash "
                 << pbft_block.getPrevBlockHash() << " in block " << pbft_block.getBlockHash();
    return false;
  }
  // TODO: Need to verify block signature
  return true;
}

void PbftChain::cleanupUnverifiedPbftBlocks(taraxa::PbftBlock const& pbft_block) {
  blk_hash_t prev_block_hash = pbft_block.getPrevBlockHash();
  upgradableLock_ lock(unverified_access_);
  if (unverified_blocks_map_.find(prev_block_hash) == unverified_blocks_map_.end()) {
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

void PbftChain::pushUnverifiedPbftBlock(std::shared_ptr<PbftBlock> const& pbft_block) {
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
               << ". Pbft unverified blocks size: " << unverified_blocks_.size();
}

std::string PbftChain::getJsonStr() const {
  Json::Value json;
  sharedLock_ lock(chain_head_access_);
  json["head_hash"] = head_hash_.toString();
  json["dag_genesis_hash"] = dag_genesis_hash_.toString();
  json["size"] = (Json::Value::UInt64)size_;
  json["last_pbft_block_hash"] = last_pbft_block_hash_.toString();
  return json.toStyledString();
}

std::ostream& operator<<(std::ostream& strm, PbftChain const& pbft_chain) {
  strm << pbft_chain.getJsonStr();
  return strm;
}

bool PbftChain::isKnownPbftBlockForSyncing(taraxa::blk_hash_t const& pbft_block_hash) {
  return findPbftBlockInSyncedSet(pbft_block_hash) || findPbftBlockInChain(pbft_block_hash);
}

uint64_t PbftChain::pbftSyncingPeriod() const {
  if (pbftSyncedQueueEmpty()) {
    return getPbftChainSize();
  } else {
    auto last_synced_votes_block = pbftSyncedQueueBack();
    return last_synced_votes_block.pbft_blk->getPeriod();
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

void PbftChain::setSyncedPbftBlockIntoQueue(PbftBlockCert const& pbft_block_and_votes) {
  LOG(log_nf_) << "Receive pbft block " << pbft_block_and_votes.pbft_blk->getBlockHash()
               << " from peer and push into synced queue";
  // NOTE: We have already checked that block is being added in order, in taraxa capability
  uniqueLock_ lock(sync_access_);
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

void PbftChain::insertUnverifiedPbftBlockIntoParentMap_(blk_hash_t const& prev_block_hash,
                                                        blk_hash_t const& block_hash) {
  upgradableLock_ lock(unverified_access_);
  if (unverified_blocks_map_.find(prev_block_hash) == unverified_blocks_map_.end()) {
    upgradeLock_ locked(lock);
    unverified_blocks_map_[prev_block_hash] = {block_hash};
  } else {
    upgradeLock_ locked(lock);
    unverified_blocks_map_[prev_block_hash].emplace_back(block_hash);
  }
}

}  // namespace taraxa
