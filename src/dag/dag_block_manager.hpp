#pragma once

#include "dag_block.hpp"
#include "final_chain/final_chain.hpp"
#include "transaction_manager/transaction.hpp"
#include "transaction_manager/transaction_manager.hpp"
#include "vdf_sortition.hpp"

namespace taraxa {

enum class BlockStatus { invalid, proposed, broadcasted };

using BlockStatusTable = ExpirationCacheMap<blk_hash_t, BlockStatus>;

// Thread safe
class DagBlockManager {
 public:
  DagBlockManager(addr_t node_addr, vdf_sortition::VdfConfig const &vdf_config,
                  optional<state_api::DPOSConfig> dpos_config, unsigned verify_threads, std::shared_ptr<DbStorage> db,
                  std::shared_ptr<TransactionManager> trx_mgr, std::shared_ptr<FinalChain> final_chain,
                  std::shared_ptr<PbftChain> pbft_chain, logger::Logger log_time_, uint32_t queue_limit = 0);
  ~DagBlockManager();
  void insertBlock(DagBlock const &blk);
  /**
   * @brief Synchronously process synced dag block with transactions and inserts it into db
   *
   * @param blk
   * @param transactions
   */
  void processSyncedBlock(DbStorage::Batch &batch, SyncBlock const &sync_block);
  void insertBroadcastedBlockWithTransactions(DagBlock const &blk, std::vector<Transaction> const &transactions);
  void pushUnverifiedBlock(DagBlock const &block, bool critical, std::vector<Transaction> const &transactions = {});
  std::pair<std::shared_ptr<DagBlock>, bool> popVerifiedBlock(bool level_limit = false,
                                                              uint64_t level = 0);  // get one verified block and pop
  void pushVerifiedBlock(DagBlock const &blk);
  std::pair<size_t, size_t> getDagBlockQueueSize() const;
  level_t getMaxDagLevelInQueue() const;
  void start();
  void stop();

  /**
   * @param hash
   * @return true in case block was already seen or is part of dag structure
   */
  bool isDagBlockKnown(blk_hash_t const &hash);

  /**
   * @brief Mark block as seen
   *
   * @param dag_block
   * @return true in case block was actually marked as seen(was not seen before), otherwise false (was already seen)
   */
  bool markDagBlockAsSeen(const DagBlock &dag_block);

  std::shared_ptr<DagBlock> getDagBlock(blk_hash_t const &hash) const;
  void clearBlockStatausTable() { blk_status_.clear(); }
  bool pivotAndTipsValid(DagBlock const &blk);
  uint64_t getCurrentMaxProposalPeriod() const;
  uint64_t getLastProposalPeriod() const;
  void setLastProposalPeriod(uint64_t const period);
  std::pair<uint64_t, bool> getProposalPeriod(level_t level);
  std::shared_ptr<ProposalPeriodDagLevelsMap> newProposePeriodDagLevelsMap(level_t anchor_level);

 private:
  using uLock = boost::unique_lock<boost::shared_mutex>;
  using sharedLock = boost::shared_lock<boost::shared_mutex>;

  void verifyBlock();
  void markBlockInvalid(blk_hash_t const &hash);

  std::atomic<bool> stopped_ = true;
  size_t num_verifiers_ = 4;
  const uint32_t cache_max_size_ = 10000;
  const uint32_t cache_delete_step_ = 100;
  std::atomic<uint64_t> last_proposal_period_ = 0;
  uint64_t current_max_proposal_period_ = 0;

  std::shared_ptr<DbStorage> db_;
  std::shared_ptr<TransactionManager> trx_mgr_;
  std::shared_ptr<FinalChain> final_chain_;
  std::shared_ptr<PbftChain> pbft_chain_;
  logger::Logger log_time_;
  // seen blks
  BlockStatusTable blk_status_;
  ExpirationCacheMap<blk_hash_t, DagBlock> seen_blocks_;
  std::vector<std::thread> verifiers_;
  mutable boost::shared_mutex shared_mutex_for_unverified_qu_;
  mutable boost::shared_mutex shared_mutex_for_verified_qu_;

  boost::condition_variable_any cond_for_unverified_qu_;
  boost::condition_variable_any cond_for_verified_qu_;
  uint32_t queue_limit_;

  std::map<uint64_t, std::deque<std::pair<DagBlock, std::vector<Transaction>>>> unverified_qu_;
  std::map<uint64_t, std::deque<DagBlock>> verified_qu_;

  vdf_sortition::VdfConfig vdf_config_;
  optional<state_api::DPOSConfig> dpos_config_;

  LOG_OBJECTS_DEFINE
};

}  // namespace taraxa
