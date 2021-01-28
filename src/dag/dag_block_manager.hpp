#pragma once

#include "chain/final_chain.hpp"
#include "dag_block.hpp"
#include "transaction_manager/transaction.hpp"
#include "transaction_manager/transaction_manager.hpp"
#include "vdf_sortition.hpp"

namespace taraxa {

enum class BlockStatus { invalid, proposed, broadcasted, verified, unseen };

using BlockStatusTable = ExpirationCacheMap<blk_hash_t, BlockStatus>;

// Thread safe
class DagBlockManager {
 public:
  DagBlockManager(addr_t node_addr, vdf_sortition::VdfConfig const &vdf_config,
                  optional<state_api::DPOSConfig> dpos_config, size_t capacity, unsigned verify_threads,
                  std::shared_ptr<DbStorage> db, std::shared_ptr<TransactionManager> trx_mgr,
                  std::shared_ptr<FinalChain> final_chain, std::shared_ptr<PbftChain> pbft_chain,
                  logger::Logger log_time_, uint32_t queue_limit = 0);
  ~DagBlockManager();
  void insertBlock(DagBlock const &blk);
  // Only used in initial syncs when blocks are received with full list of transactions
  void insertBroadcastedBlockWithTransactions(DagBlock const &blk, std::vector<Transaction> const &transactions);
  void pushUnverifiedBlock(DagBlock const &block,
                           bool critical);  // add to unverified queue
  void pushUnverifiedBlock(DagBlock const &block, std::vector<Transaction> const &transactions,
                           bool critical);  // add to unverified queue
  DagBlock popVerifiedBlock();              // get one verified block and pop
  void pushVerifiedBlock(DagBlock const &blk);
  std::pair<size_t, size_t> getDagBlockQueueSize() const;
  level_t getMaxDagLevelInQueue() const;
  void start();
  void stop();
  bool isBlockKnown(blk_hash_t const &hash);
  std::shared_ptr<DagBlock> getDagBlock(blk_hash_t const &hash) const;
  void clearBlockStatausTable() { blk_status_.clear(); }
  bool pivotAndTipsValid(DagBlock const &blk);
  uint64_t getPeriod(level_t level);

 private:
  using uLock = boost::unique_lock<boost::shared_mutex>;
  using sharedLock = boost::shared_lock<boost::shared_mutex>;
  using upgradableLock = boost::upgrade_lock<boost::shared_mutex>;
  using upgradeLock = boost::upgrade_to_unique_lock<boost::shared_mutex>;

  void verifyBlock();

  std::atomic<bool> stopped_ = true;
  size_t capacity_ = 2048;
  size_t num_verifiers_ = 4;
  const uint32_t cache_max_size = 10000;
  const uint32_t cache_delete_step = 100;

  std::shared_ptr<DbStorage> db_;
  std::shared_ptr<TransactionManager> trx_mgr_;
  std::shared_ptr<FinalChain> final_chain_;
  std::shared_ptr<PbftChain> pbft_chain_;
  logger::Logger log_time_;
  // seen blks
  BlockStatusTable blk_status_;
  ExpirationCacheMap<blk_hash_t, DagBlock> seen_blocks_;
  mutable boost::shared_mutex shared_mutex_;  // shared mutex to check seen_blocks ...
  std::vector<std::thread> verifiers_;
  mutable boost::shared_mutex shared_mutex_for_unverified_qu_;
  mutable boost::shared_mutex shared_mutex_for_verified_qu_;

  boost::condition_variable_any cond_for_unverified_qu_;
  boost::condition_variable_any cond_for_verified_qu_;
  uint32_t queue_limit_;

  std::map<uint64_t, std::deque<std::pair<DagBlock, std::vector<Transaction> > > > unverified_qu_;
  std::map<uint64_t, std::deque<DagBlock> > verified_qu_;

  vdf_sortition::VdfConfig vdf_config_;
  optional<state_api::DPOSConfig> dpos_config_;

  LOG_OBJECTS_DEFINE;
};

}  // namespace taraxa
