#include "dag/dag_block_manager.hpp"

#include "dag/dag.hpp"

namespace taraxa {

DagBlockManager::DagBlockManager(addr_t node_addr, const SortitionConfig &sortition_config, const DagConfig &dag_config,
                                 std::shared_ptr<DbStorage> db, std::shared_ptr<TransactionManager> trx_mgr,
                                 std::shared_ptr<FinalChain> final_chain, std::shared_ptr<PbftChain> pbft_chain,
                                 uint32_t queue_limit, uint32_t max_levels_per_period)
    : db_(db),
      trx_mgr_(trx_mgr),
      final_chain_(final_chain),
      pbft_chain_(pbft_chain),
      invalid_blocks_(cache_max_size_, cache_delete_step_),
      seen_blocks_(cache_max_size_, cache_delete_step_),
      queue_limit_(queue_limit),
      sortition_params_manager_(node_addr, sortition_config, db_),
      dag_config_(dag_config) {
  LOG_OBJECTS_CREATE("DAGBLKMGR");

  // Set DAG level proposal period map
  if (!db_->getProposalPeriodForDagLevel(max_levels_per_period)) {
    // Node start from scratch
    db_->saveProposalPeriodDagLevelsMap(max_levels_per_period, 0);
  }
}

DagBlockManager::~DagBlockManager() { stop(); }

void DagBlockManager::stop() {
  stopped_ = true;
  cond_for_verified_qu_.notify_all();
}

bool DagBlockManager::isDagBlockKnown(const blk_hash_t &hash) {
  auto known = seen_blocks_.count(hash);
  if (!known) return db_->dagBlockInDb(hash);
  return true;
}

bool DagBlockManager::markDagBlockAsSeen(const DagBlock &dag_block) {
  return seen_blocks_.insert(dag_block.getHash(), dag_block);
}

std::shared_ptr<DagBlock> DagBlockManager::getDagBlock(const blk_hash_t &hash) const {
  auto blk = seen_blocks_.get(hash);
  if (blk.second) {
    return std::make_shared<DagBlock>(blk.first);
  }

  return db_->getDagBlock(hash);
}

bool DagBlockManager::pivotAndTipsValid(const DagBlock &blk) {
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

DagBlockManager::InsertAndVerifyBlockReturnType DagBlockManager::insertAndVerifyBlock(DagBlock &&blk) {
  if (isDagBlockKnown(blk.getHash())) {
    LOG(log_dg_) << "Trying to push new unverified block " << blk.getHash().abridged()
                 << " that is already known, skip it";
    return InsertAndVerifyBlockReturnType::AlreadyKnown;
  }

  // Mark block as seen - synchronization point in case multiple threads are processing the same block at the same time
  if (!markDagBlockAsSeen(blk)) {
    LOG(log_dg_) << "Trying to push new unverified block " << blk.getHash().abridged()
                 << " that is already marked as known, skip it";
    return InsertAndVerifyBlockReturnType::AlreadyKnown;
  }

  if (queue_limit_ > 0) {
    if (const auto queue_size = getDagBlockQueueSize(); queue_limit_ < queue_size) {
      LOG(log_er_) << "Block queue large. Verified queue: " << queue_size << "; Limit: " << queue_limit_;
    }
    return InsertAndVerifyBlockReturnType::BlockQueueOverflow;
  }
  const auto verified = verifyBlock(blk);
  if (verified == InsertAndVerifyBlockReturnType::InsertedAndVerified) {
    {
      uLock lock(shared_mutex_for_verified_qu_);
      verified_qu_[blk.getLevel()].push_back(std::move(blk));
    }
    cond_for_verified_qu_.notify_one();
  }
  return verified;
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

void DagBlockManager::pushVerifiedBlock(const DagBlock &blk) {
  uLock lock(shared_mutex_for_verified_qu_);
  verified_qu_[blk.getLevel()].emplace_back(blk);
}

DagBlockManager::InsertAndVerifyBlockReturnType DagBlockManager::verifyBlock(const DagBlock &blk) {
  const auto &block_hash = blk.getHash();

  if (invalid_blocks_.count(block_hash)) {
    LOG(log_wr_) << "Skip invalid DAG block " << block_hash;
    return InsertAndVerifyBlockReturnType::InvalidBlock;
  }

  // Verify transactions
  if (!trx_mgr_->checkBlockTransactions(blk)) {
    LOG(log_nf_) << "Ignore block " << block_hash << " since it has missing transactions";
    // This can be a valid block so just remove it from the seen list
    seen_blocks_.erase(block_hash);
    return InsertAndVerifyBlockReturnType::MissingTransaction;
  }

  auto propose_period = db_->getProposalPeriodForDagLevel(blk.getLevel());
  // Verify DPOS
  if (!propose_period) {
    // Cannot find the proposal period in DB yet. The slow node gets an ahead block, remove from seen_blocks
    LOG(log_nf_) << "Cannot find proposal period in DB for DAG block " << blk.getHash();
    seen_blocks_.erase(block_hash);
    return InsertAndVerifyBlockReturnType::AheadBlock;
  }
  if (blk.getLevel() < dag_expiry_level_) {
    LOG(log_nf_) << "Dropping old block: " << blk.getHash() << ". Expiry level: " << dag_expiry_level_
                 << ". Block level: " << blk.getLevel();
    return InsertAndVerifyBlockReturnType::ExpiredBlock;
  }

  // Verify VDF solution
  const auto proposal_period_hash = db_->getPeriodBlockHash(*propose_period);
  try {
    blk.verifyVdf(sortition_params_manager_.getSortitionParams(*propose_period), proposal_period_hash);
  } catch (vdf_sortition::VdfSortition::InvalidVdfSortition const &e) {
    LOG(log_er_) << "DAG block " << block_hash << " with " << blk.getLevel()
                 << " level failed on VDF verification with pivot hash " << blk.getPivot() << " reason " << e.what();
    LOG(log_er_) << "period from map: " << *propose_period << " current: " << pbft_chain_->getPbftChainSize();
    markBlockInvalid(block_hash);
    return InsertAndVerifyBlockReturnType::FailedVdfVerification;
  }

  auto dag_block_sender = blk.getSender();
  bool dpos_qualified;
  try {
    dpos_qualified = final_chain_->dpos_is_eligible(*propose_period, dag_block_sender);
  } catch (state_api::ErrFutureBlock &c) {
    LOG(log_er_) << "Verify proposal period " << *propose_period << " is too far ahead of DPOS. " << c.what();
    return InsertAndVerifyBlockReturnType::FutureBlock;
  }
  if (!dpos_qualified) {
    LOG(log_er_) << "Invalid DAG block DPOS. DAG block " << blk << " is not eligible for DPOS at period "
                 << *propose_period << " for sender " << dag_block_sender.toString() << " current period "
                 << final_chain_->last_block_number();
    markBlockInvalid(block_hash);
    return InsertAndVerifyBlockReturnType::NotEligible;
  }
  {
    u256 total_block_weight = 0;
    const auto &trxs = blk.getTrxs();
    const auto &trxs_gas_estimations = blk.getTrxsGasEstimations();
    if (trxs.size() != trxs_gas_estimations.size()) {
      return InsertAndVerifyBlockReturnType::IncorrectTransactionsEstimation;
    }
    for (size_t i = 0; i < trxs.size(); ++i) {
      const auto &e = trx_mgr_->estimateTransactionGasByHash(trxs[i], propose_period);
      if (e != trxs_gas_estimations[i]) {
        return InsertAndVerifyBlockReturnType::IncorrectTransactionsEstimation;
      }
      total_block_weight += e;
    }
    if (total_block_weight > getDagConfig().gas_limit) {
      return InsertAndVerifyBlockReturnType::BlockTooBig;
    }
  }

  return InsertAndVerifyBlockReturnType::InsertedAndVerified;
}

void DagBlockManager::markBlockInvalid(const blk_hash_t &hash) {
  invalid_blocks_.insert(hash);
  seen_blocks_.erase(hash);
}

}  // namespace taraxa
