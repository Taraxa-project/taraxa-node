#include "dag_block_manager.hpp"

namespace taraxa {

DagBlockManager::DagBlockManager(addr_t node_addr, vdf_sortition::VdfConfig const &vdf_config,
                                 optional<state_api::DPOSConfig> dpos_config, unsigned num_verifiers,
                                 std::shared_ptr<DbStorage> db, std::shared_ptr<TransactionManager> trx_mgr,
                                 std::shared_ptr<FinalChain> final_chain, std::shared_ptr<PbftChain> pbft_chain,
                                 logger::Logger log_time, uint32_t queue_limit)
    : num_verifiers_(num_verifiers),
      db_(db),
      trx_mgr_(trx_mgr),
      final_chain_(final_chain),
      pbft_chain_(pbft_chain),
      log_time_(log_time),
      blk_status_(cache_max_size_, cache_delete_step_),
      seen_blocks_(cache_max_size_, cache_delete_step_),
      queue_limit_(queue_limit),
      vdf_config_(vdf_config),
      dpos_config_(dpos_config) {
  LOG_OBJECTS_CREATE("BLKQU");

  // Set DAG level proposal period map
  current_max_proposal_period_ =
      db_->getDposProposalPeriodLevelsField(DposProposalPeriodLevelsStatus::max_proposal_period);
  last_proposal_period_ = current_max_proposal_period_;
  if (current_max_proposal_period_ == 0) {
    // Node start from scratch
    ProposalPeriodDagLevelsMap period_levels_map;
    db_->saveProposalPeriodDagLevelsMap(period_levels_map);
  }
}

DagBlockManager::~DagBlockManager() { stop(); }

void DagBlockManager::start() {
  if (bool b = true; !stopped_.compare_exchange_strong(b, !b)) {
    return;
  }
  LOG(log_nf_) << "Create verifier threads = " << num_verifiers_ << std::endl;
  verifiers_.clear();
  for (size_t i = 0; i < num_verifiers_; ++i) {
    verifiers_.emplace_back([this]() { this->verifyBlock(); });
  }
}

void DagBlockManager::stop() {
  if (bool b = false; !stopped_.compare_exchange_strong(b, !b)) {
    return;
  }
  cond_for_unverified_qu_.notify_all();
  cond_for_verified_qu_.notify_all();
  for (auto &t : verifiers_) {
    t.join();
  }
}

bool DagBlockManager::isBlockKnown(blk_hash_t const &hash) {
  auto known = seen_blocks_.count(hash);
  if (!known) return getDagBlock(hash) != nullptr;
  return true;
}

std::shared_ptr<DagBlock> DagBlockManager::getDagBlock(blk_hash_t const &hash) const {
  auto blk = seen_blocks_.get(hash);
  if (blk.second) {
    return std::make_shared<DagBlock>(blk.first);
  }

  return db_->getDagBlock(hash);
}

bool DagBlockManager::pivotAndTipsValid(DagBlock const &blk) {
  auto status = blk_status_.get(blk.getPivot());
  if (status.second && status.first == BlockStatus::invalid) {
    blk_status_.update(blk.getHash(), BlockStatus::invalid);
    LOG(log_dg_) << "DAG Block " << blk.getHash() << " pivot " << blk.getPivot() << " unavailable";
    return false;
  }
  for (auto const &t : blk.getTips()) {
    auto status = blk_status_.get(t);
    if (status.second && status.first == BlockStatus::invalid) {
      blk_status_.update(blk.getHash(), BlockStatus::invalid);
      LOG(log_dg_) << "DAG Block " << blk.getHash() << " tip " << t << " unavailable";
      return false;
    }
  }
  return true;
}

level_t DagBlockManager::getMaxDagLevelInQueue() const {
  level_t max_level = 0;
  {
    sharedLock lock(shared_mutex_for_unverified_qu_);
    if (unverified_qu_.size() != 0) max_level = unverified_qu_.rbegin()->first;
  }
  {
    sharedLock lock(shared_mutex_for_verified_qu_);
    if (verified_qu_.size() != 0) max_level = std::max(verified_qu_.rbegin()->first, max_level);
  }
  return max_level;
}

void DagBlockManager::insertBlock(DagBlock const &blk) {
  if (isBlockKnown(blk.getHash())) {
    LOG(log_nf_) << "Block known " << blk.getHash();
    return;
  }
  pushUnverifiedBlock(std::move(blk), true /*critical*/);
  LOG(log_time_) << "Store cblock " << blk.getHash() << " at: " << getCurrentTimeMilliSeconds()
                 << " ,trxs: " << blk.getTrxs().size() << " , tips: " << blk.getTips().size();
}

void DagBlockManager::pushUnverifiedBlock(DagBlock const &blk, std::vector<Transaction> const &transactions,
                                          bool critical) {
  if (queue_limit_ > 0) {
    auto queue_size = getDagBlockQueueSize();
    if (queue_limit_ > queue_size.first + queue_size.second) {
      LOG(log_wr_) << "Block queue large. Unverified queue: " << queue_size.first
                   << "; Verified queue: " << queue_size.second << "; Limit: " << queue_limit_;
    }
  }
  seen_blocks_.update(blk.getHash(), blk);
  {
    uLock lock(shared_mutex_for_unverified_qu_);
    if (critical) {
      blk_status_.insert(blk.getHash(), BlockStatus::proposed);

      unverified_qu_[blk.getLevel()].emplace_front(std::make_pair(blk, transactions));
      LOG(log_dg_) << "Insert unverified block from front: " << blk.getHash() << std::endl;
    } else {
      blk_status_.insert(blk.getHash(), BlockStatus::broadcasted);
      unverified_qu_[blk.getLevel()].emplace_back(std::make_pair(blk, transactions));
      LOG(log_dg_) << "Insert unverified block from back: " << blk.getHash() << std::endl;
    }
  }
  cond_for_unverified_qu_.notify_one();
}

void DagBlockManager::insertBroadcastedBlockWithTransactions(DagBlock const &blk,
                                                             std::vector<Transaction> const &transactions) {
  if (isBlockKnown(blk.getHash())) {
    LOG(log_dg_) << "Block known " << blk.getHash();
    return;
  }
  pushUnverifiedBlock(blk, transactions, false /*critical*/);
  LOG(log_time_) << "Store ncblock " << blk.getHash() << " at: " << getCurrentTimeMilliSeconds()
                 << " ,trxs: " << blk.getTrxs().size() << " , tips: " << blk.getTips().size();
}

void DagBlockManager::pushUnverifiedBlock(DagBlock const &blk, bool critical) {
  pushUnverifiedBlock(blk, std::vector<Transaction>(), critical);
}

std::pair<size_t, size_t> DagBlockManager::getDagBlockQueueSize() const {
  std::pair<size_t, size_t> res;
  {
    sharedLock lock(shared_mutex_for_unverified_qu_);
    res.first = 0;
    for (auto &it : unverified_qu_) {
      res.first += it.second.size();
    }
  }

  {
    sharedLock lock(shared_mutex_for_verified_qu_);
    res.second = 0;
    for (auto &it : verified_qu_) {
      res.second += it.second.size();
    }
  }
  return res;
}

DagBlock DagBlockManager::popVerifiedBlock() {
  uLock lock(shared_mutex_for_verified_qu_);
  while (verified_qu_.empty() && !stopped_) {
    cond_for_verified_qu_.wait(lock);
  }
  if (stopped_) return DagBlock();

  auto blk = verified_qu_.begin()->second.front();
  verified_qu_.begin()->second.pop_front();
  if (verified_qu_.begin()->second.empty()) verified_qu_.erase(verified_qu_.begin());
  return blk;
}

void DagBlockManager::pushVerifiedBlock(DagBlock const &blk) {
  uLock lock(shared_mutex_for_verified_qu_);
  verified_qu_[blk.getLevel()].emplace_back(blk);
}

void DagBlockManager::processSyncedBlockWithTransactions(const DagBlock& blk, const std::vector<Transaction>& txs) {
  blk_hash_t block_hash = blk.getHash();

  // This dag block was already processed, skip it
  if (isBlockKnown(block_hash)) {
    LOG(log_dg_) << "Skipping dag block " << block_hash << " -> already processed.";
    return;
  }

  if (queue_limit_ > 0) {
    auto queue_size = getDagBlockQueueSize();
    if (queue_limit_ > queue_size.first + queue_size.second) {
      LOG(log_wr_) << "Block queue large. Unverified queue: " << queue_size.first
                   << "; Verified queue: " << queue_size.second << "; Limit: " << queue_limit_;
    }
  }
  seen_blocks_.update(block_hash, blk);
  blk_status_.insert(block_hash, BlockStatus::broadcasted);

  // Check if there is at least 1 tx in dag block - should never happen that it is not
  vec_trx_t const &all_block_trx_hashes = blk.getTrxs();
  if (all_block_trx_hashes.empty()) {
    LOG(log_er_) << "Ignore block " << block_hash << " since it has no transactions";
    blk_status_.update(block_hash, BlockStatus::invalid);
    return;
  }

  std::set<trx_hash_t> known_trx_hashes(all_block_trx_hashes.begin(), all_block_trx_hashes.end());

  // Filter known txs + save unseen txs to the db
  auto trx_batch = db_->createWriteBatch();
  for (auto const &trx : txs) {
    known_trx_hashes.erase(trx.getHash());

    if (db_->getTransactionStatus(trx.getHash()) == TransactionStatus::not_seen) {
      db_->addTransactionToBatch(trx, trx_batch);
    }
  }
  db_->commitWriteBatch(trx_batch);

  // Check if all known txs have really been already seen
  for (auto const &trx : known_trx_hashes) {
    if (db_->getTransactionStatus(trx) == TransactionStatus::not_seen) {
      LOG(log_er_) << "Ignore block " << block_hash << " since it has missing transaction " << trx;
      blk_status_.update(block_hash, BlockStatus::invalid);
      return;
    }
  }

  // Update txs status to TransactionStatus::in_block
  size_t newly_added_txs_to_block_counter = 0;
  trx_batch = db_->createWriteBatch();
  for (auto const &trx : all_block_trx_hashes) {
    if (db_->getTransactionStatus(trx) != TransactionStatus::in_block) {
      db_->addTransactionStatusToBatch(trx_batch, trx, TransactionStatus::in_block);
      newly_added_txs_to_block_counter++;
    }
  }
  trx_mgr_->addTrxCount(newly_added_txs_to_block_counter);

  // Write prepared batch to db
  db_->addStatusFieldToBatch(StatusDbField::TrxCount, trx_mgr_->getTransactionCount(), trx_batch);
  db_->commitWriteBatch(trx_batch);

  trx_mgr_->getTransactionQueue().removeBlockTransactionsFromQueue(all_block_trx_hashes);

  {
    uLock lock(shared_mutex_for_verified_qu_);
    verified_qu_[blk.getLevel()].emplace_back(blk);
  }
  blk_status_.update(block_hash, BlockStatus::verified);

  LOG(log_dg_) << "Synced dag block: " << block_hash;
  cond_for_verified_qu_.notify_one();
}

void DagBlockManager::verifyBlock() {
  while (!stopped_) {
    std::pair<DagBlock, std::vector<Transaction>> blk;
    {
      uLock lock(shared_mutex_for_unverified_qu_);
      while (unverified_qu_.empty() && !stopped_) {
        cond_for_unverified_qu_.wait(lock);
      }
      if (stopped_) {
        return;
      }
      blk = unverified_qu_.begin()->second.front();
      unverified_qu_.begin()->second.pop_front();
      if (unverified_qu_.begin()->second.empty()) {
        unverified_qu_.erase(unverified_qu_.begin());
      }
    }
    auto status = blk_status_.get(blk.first.getHash());
    // Verifying transaction ...
    LOG(log_time_) << "Verifying Trx block  " << blk.first.getHash() << " at: " << getCurrentTimeMilliSeconds();
    // only need to verify if this is a broadcasted block (proposed block are generated by verified trx)
    if (!status.second || status.first == BlockStatus::broadcasted) {
      // Verify transactions
      if (!trx_mgr_->verifyBlockTransactions(blk.first, blk.second)) {
        LOG(log_er_) << "Ignore block " << blk.first.getHash() << " since it has invalid or missing transactions";
        blk_status_.update(blk.first.getHash(), BlockStatus::invalid);
        continue;
      }

      // Verify VDF solution
      vdf_sortition::VdfSortition vdf = blk.first.getVdf();
      try {
        vdf.verifyVdf(vdf_config_, getRlpBytes(blk.first.getLevel()), blk.first.getPivot().asBytes());
      } catch (vdf_sortition::VdfSortition::InvalidVdfSortition const &e) {
        LOG(log_er_) << "DAG block " << blk.first.getHash() << " failed on VDF verification with pivot hash "
                     << blk.first.getPivot() << " reason " << e.what();
        blk_status_.update(blk.first.getHash(), BlockStatus::invalid);
        continue;
      }

      // Verify DPOS
      auto propose_period = getProposalPeriod(blk.first.getLevel());
      if (!propose_period.second) {
        // Cannot find the proposal period in DB yet. The slow node gets an ahead block, puts back.
        LOG(log_nf_) << "Cannot find proposal period " << propose_period.first << " in DB for DAG block " << blk.first;
        uLock lock(shared_mutex_for_unverified_qu_);
        unverified_qu_[blk.first.getLevel()].emplace_back(blk);
        continue;
      }
      auto dag_block_sender = blk.first.getSender();
      bool dpos_qualified;
      try {
        dpos_qualified = final_chain_->dpos_is_eligible(propose_period.first, dag_block_sender);
      } catch (state_api::ErrFutureBlock &c) {
        LOG(log_er_) << "Verify proposal period " << propose_period.first << " is too far ahead of DPOS. " << c.what();
        uLock lock(shared_mutex_for_unverified_qu_);
        unverified_qu_[blk.first.getLevel()].emplace_back(blk);
        continue;
      }
      if (!dpos_qualified) {
        auto executed_period = final_chain_->last_block_number();
        auto dpos_period = executed_period;
        if (dpos_config_) {
          dpos_period += dpos_config_->deposit_delay;
        }
        if (propose_period.first <= dpos_period) {
          LOG(log_er_) << "Invalid DAG block DPOS. DAG block " << blk.first << " is not eligible for DPOS at period "
                       << propose_period.first << " for sender " << dag_block_sender.toString() << ". Executed period "
                       << executed_period << ", DPOS period " << dpos_period;
          blk_status_.update(blk.first.getHash(), BlockStatus::invalid);
        } else {
          // The incoming DAG block is ahead of DPOS period, add back in unverified queue
          uLock lock(shared_mutex_for_unverified_qu_);
          unverified_qu_[blk.first.getLevel()].emplace_back(blk);
        }
        continue;
      }
    }

    {
      uLock lock(shared_mutex_for_verified_qu_);
      if (status.second && status.first == BlockStatus::proposed) {
        verified_qu_[blk.first.getLevel()].emplace_front(blk.first);
      } else if (!status.second || status.first == BlockStatus::broadcasted) {
        verified_qu_[blk.first.getLevel()].emplace_back(blk.first);
      }
    }
    blk_status_.update(blk.first.getHash(), BlockStatus::verified);

    LOG(log_time_) << "VerifiedTrx stored " << blk.first.getHash() << " at: " << getCurrentTimeMilliSeconds();

    cond_for_verified_qu_.notify_one();
    LOG(log_dg_) << "Verified block: " << blk.first.getHash() << std::endl;
  }
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
      // propsoal level less than the interval
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

}  // namespace taraxa