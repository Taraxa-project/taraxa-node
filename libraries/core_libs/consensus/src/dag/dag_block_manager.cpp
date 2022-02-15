#include "dag/dag_block_manager.hpp"

#include "dag/dag.hpp"

namespace taraxa {

DagBlockManager::DagBlockManager(addr_t node_addr, SortitionConfig const &sortition_config,
                                 std::shared_ptr<DbStorage> db, std::shared_ptr<TransactionManager> trx_mgr,
                                 std::shared_ptr<FinalChain> final_chain, std::shared_ptr<PbftChain> pbft_chain,
                                 logger::Logger log_time, uint32_t queue_limit)
    : db_(db),
      trx_mgr_(trx_mgr),
      final_chain_(final_chain),
      pbft_chain_(pbft_chain),
      log_time_(log_time),
      invalid_blocks_(cache_max_size_, cache_delete_step_),
      seen_blocks_(cache_max_size_, cache_delete_step_),
      queue_limit_(queue_limit),
      sortition_params_manager_(node_addr, sortition_config, db_) {
  LOG_OBJECTS_CREATE("DAGBLKMGR");

  // Set DAG level proposal period map
  current_max_proposal_period_ =
      db_->getDposProposalPeriodLevelsField(DposProposalPeriodLevelsStatus::MaxProposalPeriod);
  last_proposal_period_ = current_max_proposal_period_;
  if (current_max_proposal_period_ == 0) {
    // Node start from scratch
    ProposalPeriodDagLevelsMap period_levels_map;
    db_->saveProposalPeriodDagLevelsMap(period_levels_map);
  }
}

DagBlockManager::~DagBlockManager() { stop(); }

void DagBlockManager::stop() {
  stopped_ = true;
  cond_for_verified_qu_.notify_all();
}

bool DagBlockManager::isDagBlockKnown(blk_hash_t const &hash) {
  auto known = seen_blocks_.count(hash);
  if (!known) return getDagBlock(hash) != nullptr;
  return true;
}

bool DagBlockManager::markDagBlockAsSeen(const DagBlock &dag_block) {
  return seen_blocks_.insert(dag_block.getHash(), dag_block);
}

std::shared_ptr<DagBlock> DagBlockManager::getDagBlock(blk_hash_t const &hash) const {
  auto blk = seen_blocks_.get(hash);
  if (blk.second) {
    return std::make_shared<DagBlock>(blk.first);
  }

  return db_->getDagBlock(hash);
}

bool DagBlockManager::pivotAndTipsValid(DagBlock const &blk) {
  // Check pivot validation
  if (invalid_blocks_.count(blk.getPivot())) {
    invalid_blocks_.insert(blk.getHash());
    seen_blocks_.erase(blk.getHash());
    LOG(log_wr_) << "DAG Block " << blk.getHash() << " pivot " << blk.getPivot() << " is invalid";
    return false;
  }

  if (seen_blocks_.count(blk.getPivot()) == 0 && !db_->dagBlockInDb(blk.getPivot())) {
    seen_blocks_.erase(blk.getHash());
    LOG(log_wr_) << "DAG Block " << blk.getHash() << " pivot " << blk.getPivot() << " is unavailable";
    return false;
  }

  for (auto const &tip : blk.getTips()) {
    if (invalid_blocks_.count(tip)) {
      invalid_blocks_.insert(blk.getHash());
      seen_blocks_.erase(blk.getHash());
      LOG(log_wr_) << "DAG Block " << blk.getHash() << " tip " << tip << " is invalid";
      return false;
    }
  }

  for (auto const &tip : blk.getTips()) {
    if (seen_blocks_.count(tip) == 0 && !db_->dagBlockInDb(tip)) {
      seen_blocks_.erase(blk.getHash());
      LOG(log_wr_) << "DAG Block " << blk.getHash() << " tip " << tip << " is unavailable";
      return false;
    }
  }

  return true;
}

level_t DagBlockManager::getMaxDagLevelInQueue() const {
  level_t max_level = 0;
  {
    sharedLock lock(shared_mutex_for_verified_qu_);
    if (verified_qu_.size() != 0) max_level = std::max(verified_qu_.rbegin()->first, max_level);
  }
  return max_level;
}

void DagBlockManager::insertAndVerifyBlock(DagBlock &&blk) {
  if (isDagBlockKnown(blk.getHash())) {
    LOG(log_dg_) << "Trying to push new unverified block " << blk.getHash().abridged()
                 << " that is already known, skip it";
    return;
  }

  // Mark block as seen - synchronization point in case multiple threads are processing the same block at the same time
  if (!markDagBlockAsSeen(blk)) {
    LOG(log_dg_) << "Trying to push new unverified block " << blk.getHash().abridged()
                 << " that is already marked as known, skip it";
    return;
  }

  if (queue_limit_ > 0) {
    if (const auto queue_size = getDagBlockQueueSize(); queue_limit_ < queue_size) {
      LOG(log_wr_) << "Block queue large. Verified queue: " << queue_size << "; Limit: " << queue_limit_;
    }
    // TODO return ?
  }
  if (verifyBlock(blk)) {
    {
      uLock lock(shared_mutex_for_verified_qu_);
      verified_qu_[blk.getLevel()].push_back(std::move(blk));
    }
    cond_for_verified_qu_.notify_one();
  }
}

size_t DagBlockManager::getDagBlockQueueSize() const {
  size_t res = 0;
  {
    sharedLock lock(shared_mutex_for_verified_qu_);
    for (auto &it : verified_qu_) {
      res += it.second.size();
    }
  }
  return res;
}

std::optional<DagBlock> DagBlockManager::popVerifiedBlock(bool level_limit, uint64_t level) {
  uLock lock(shared_mutex_for_verified_qu_);
  if (level_limit) {
    while ((verified_qu_.empty() || verified_qu_.begin()->first >= level) && !stopped_) {
      cond_for_verified_qu_.wait(lock);
    }
  } else {
    while (verified_qu_.empty() && !stopped_) {
      cond_for_verified_qu_.wait(lock);
    }
  }
  if (stopped_) return {};

  auto blk = std::move(verified_qu_.begin()->second.front());
  verified_qu_.begin()->second.pop_front();
  if (verified_qu_.begin()->second.empty()) verified_qu_.erase(verified_qu_.begin());

  return blk;
}

void DagBlockManager::pushVerifiedBlock(DagBlock const &blk) {
  uLock lock(shared_mutex_for_verified_qu_);
  verified_qu_[blk.getLevel()].emplace_back(blk);
}

bool DagBlockManager::verifyBlock(const DagBlock &blk) {
  const auto &block_hash = blk.getHash();
  LOG(log_time_) << "Verifying Trx block  " << block_hash << " at: " << getCurrentTimeMilliSeconds();

  if (invalid_blocks_.count(block_hash)) {
    LOG(log_wr_) << "Skip invalid DAG block " << block_hash;
    return false;
  }

  // Verify transactions
  if (!trx_mgr_->checkBlockTransactions(blk)) {
    LOG(log_er_) << "Ignore block " << block_hash << " since it has missing transactions";
    // This can be a valid block so just remove it from the seen list
    seen_blocks_.erase(block_hash);
    return false;
  }

  auto propose_period = getProposalPeriod(blk.getLevel());
  // Verify DPOS
  if (!propose_period.second) {
    // Cannot find the proposal period in DB yet. The slow node gets an ahead block, remove from seen_blocks
    LOG(log_nf_) << "Cannot find proposal period " << propose_period.first << " in DB for DAG block " << blk.getHash();
    seen_blocks_.erase(block_hash);
    return false;
  }

  // Verify VDF solution
  const auto proposal_period_hash = db_->getPeriodBlockHash(propose_period.first);
  try {
    blk.verifyVdf(sortition_params_manager_.getSortitionParams(propose_period.first), proposal_period_hash);
  } catch (vdf_sortition::VdfSortition::InvalidVdfSortition const &e) {
    LOG(log_er_) << "DAG block " << block_hash << " with " << blk.getLevel()
                 << " level failed on VDF verification with pivot hash " << blk.getPivot() << " reason " << e.what();
    LOG(log_er_) << "period from map: " << propose_period.first << " current: " << pbft_chain_->getPbftChainSize();
    markBlockInvalid(block_hash);
    return false;
  }

  auto dag_block_sender = blk.getSender();
  bool dpos_qualified;
  try {
    dpos_qualified = final_chain_->dpos_is_eligible(propose_period.first, dag_block_sender);
  } catch (state_api::ErrFutureBlock &c) {
    LOG(log_er_) << "Verify proposal period " << propose_period.first << " is too far ahead of DPOS. " << c.what();
    return false;
  }
  if (!dpos_qualified) {
    LOG(log_er_) << "Invalid DAG block DPOS. DAG block " << blk << " is not eligible for DPOS at period "
                 << propose_period.first << " for sender " << dag_block_sender.toString() << " current period "
                 << final_chain_->last_block_number();
    markBlockInvalid(block_hash);
    return false;
  }
  return true;
}

uint64_t DagBlockManager::getCurrentMaxProposalPeriod() const { return current_max_proposal_period_; }

uint64_t DagBlockManager::getLastProposalPeriod() const { return last_proposal_period_; }

void DagBlockManager::setLastProposalPeriod(uint64_t const period) { last_proposal_period_ = period; }

std::pair<uint64_t, bool> DagBlockManager::getProposalPeriod(level_t level) {
  std::pair<uint64_t, bool> result;

  // Threads safe
  auto proposal_period = getLastProposalPeriod();
  while (true) {
    bytes period_levels_bytes = db_->getProposalPeriodDagLevelsMap(proposal_period);
    if (period_levels_bytes.empty()) {
      // Cannot find the proposal period in DB, too far ahead of proposal DAG blocks
      result = std::make_pair(proposal_period, false);
      assert(proposal_period);  // Avoid overflow
      proposal_period--;
      break;
    }

    ProposalPeriodDagLevelsMap period_levels_map(period_levels_bytes);
    if (level >= period_levels_map.levels_interval.first && level <= period_levels_map.levels_interval.second) {
      // proposal level stay in the period
      result = std::make_pair(period_levels_map.proposal_period, true);
      break;
    } else if (level > period_levels_map.levels_interval.second) {
      proposal_period++;
    } else {
      // proposal level less than the interval
      assert(proposal_period);  // Avoid overflow
      proposal_period--;
    }
  }

  setLastProposalPeriod(proposal_period);

  return result;
}

std::shared_ptr<ProposalPeriodDagLevelsMap> DagBlockManager::newProposePeriodDagLevelsMap(level_t anchor_level) {
  bytes period_levels_bytes = db_->getProposalPeriodDagLevelsMap(current_max_proposal_period_);
  assert(!period_levels_bytes.empty());
  ProposalPeriodDagLevelsMap last_period_levels_map(period_levels_bytes);

  auto propose_period = ++current_max_proposal_period_;
  auto level_start = last_period_levels_map.levels_interval.second + 1;
  auto level_end = anchor_level + last_period_levels_map.max_levels_per_period;
  assert(level_end >= level_start);
  ProposalPeriodDagLevelsMap new_period_levels_map(propose_period, level_start, level_end);

  return std::make_shared<ProposalPeriodDagLevelsMap>(new_period_levels_map);
}

void DagBlockManager::markBlockInvalid(blk_hash_t const &hash) {
  invalid_blocks_.insert(hash);
  seen_blocks_.erase(hash);
}

}  // namespace taraxa
