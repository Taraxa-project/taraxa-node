#include "dag_block_manager.hpp"

namespace taraxa {

DagBlockManager::DagBlockManager(addr_t node_addr, vdf_sortition::VdfConfig const &vdf_config,
                                 optional<state_api::DPOSConfig> dpos_config, size_t capacity, unsigned num_verifiers,
                                 std::shared_ptr<DbStorage> db, std::shared_ptr<TransactionManager> trx_mgr,
                                 std::shared_ptr<FinalChain> final_chain, std::shared_ptr<PbftChain> pbft_chain,
                                 logger::Logger log_time, uint32_t queue_limit)
    : vdf_config_(vdf_config),
      dpos_config_(dpos_config),
      capacity_(capacity),
      num_verifiers_(num_verifiers),
      db_(db),
      trx_mgr_(trx_mgr),
      final_chain_(final_chain),
      pbft_chain_(pbft_chain),
      log_time_(log_time),
      queue_limit_(queue_limit),
      blk_status_(10000, 100),
      seen_blocks_(10000, 100) {
  LOG_OBJECTS_CREATE("BLKQU");
}

DagBlockManager::~DagBlockManager() { stop(); }

void DagBlockManager::start() {
  if (bool b = true; !stopped_.compare_exchange_strong(b, !b)) {
    return;
  }
  LOG(log_nf_) << "Create verifier threads = " << num_verifiers_ << std::endl;
  verifiers_.clear();
  for (auto i = 0; i < num_verifiers_; ++i) {
    verifiers_.emplace_back([this, i]() { this->verifyBlock(); });
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
  std::shared_ptr<DagBlock> ret;
  auto blk = seen_blocks_.get(hash);
  if (blk.second) {
    ret = std::make_shared<DagBlock>(blk.first);
  }
  if (!ret) {
    return db_->getDagBlock(hash);
  }
  return ret;
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
    uLock lock(shared_mutex_for_unverified_qu_);
    if (unverified_qu_.size() != 0) max_level = unverified_qu_.rbegin()->first;
  }
  {
    uLock lock(shared_mutex_for_verified_qu_);
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
      LOG(log_wr_) << "Warning: block queue large. Unverified queue: " << queue_size.first
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
    uLock lock(shared_mutex_for_unverified_qu_);
    res.first = 0;
    for (auto &it : unverified_qu_) {
      res.first += it.second.size();
    }
  }

  {
    uLock lock(shared_mutex_for_verified_qu_);
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
      if (!vdf.verifyVdf(vdf_config_, getRlpBytes(blk.first.getLevel()), blk.first.getPivot().asBytes())) {
        LOG(log_er_) << "DAG block " << blk.first.getHash() << " failed on VDF verification with pivot hash "
                     << blk.first.getPivot();
        blk_status_.update(blk.first.getHash(), BlockStatus::invalid);
        continue;
      }
      // Verify DPOS
      auto period = getPeriod(blk.first.getLevel());
      auto dag_block_sender = blk.first.getSender();
      if (!final_chain_->dpos_is_eligible(period, dag_block_sender)) {
        auto executed_period = pbft_chain_->getPbftExecutedChainSize();
        auto dpos_period = executed_period;
        if (dpos_config_) {
          dpos_period += dpos_config_->deposit_delay;
        }
        if (period <= dpos_period) {
          LOG(log_er_) << "Invalid DAG block DPOS. DAG block " << blk.first << " is not eligible for DPOS at period "
                       << period << " for sender " << dag_block_sender.toString() << ". Executed period "
                       << executed_period << ", DPOS period " << dpos_period;
          blk_status_.update(blk.first.getHash(), BlockStatus::invalid);
        } else {
          // The DAG block is ahead of DPOS period, add back in unverified queue
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

uint64_t DagBlockManager::getPeriod(level_t level) {
  // TODO: Use DAG block level map to period
  return 0;
}

}  // namespace taraxa