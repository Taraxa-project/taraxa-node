#pragma once

#include "config/state_api_config.hpp"
#include "dag/dag_block.hpp"
#include "dag/sortition_params_manager.hpp"
#include "final_chain/final_chain.hpp"
#include "pbft/pbft_chain.hpp"
#include "transaction/transaction_manager.hpp"
#include "vdf/sortition.hpp"

namespace taraxa {
/** @addtogroup DAG
 * @{
 */

/**
 * @brief Thread safe
 */
class DagBlockManager {
 public:
  /**
   * @brief return type of insertAndVerifyBlock
   */
  enum class InsertAndVerifyBlockReturnType : uint32_t {
    InsertedAndVerified = 0,
    AlreadyKnown,
    BlockQueueOverflow,
    InvalidBlock,
    MissingTransaction,
    AheadBlock,
    FailedVdfVerification,
    FutureBlock,
    NotEligible,
    ExpiredBlock,
    IncorrectTransactionsEstimation,
    BlockTooBig
  };

  DagBlockManager(addr_t node_addr, const SortitionConfig &sortition_config, const DagConfig &dag_config,
                  std::shared_ptr<DbStorage> db, std::shared_ptr<TransactionManager> trx_mgr,
                  std::shared_ptr<FinalChain> final_chain, std::shared_ptr<PbftChain> pbft_chain,
                  logger::Logger log_time_, uint32_t queue_limit = 0,
                  uint32_t max_levels_per_period = kMaxLevelsPerPeriod);
  ~DagBlockManager();

  DagBlockManager(const DagBlockManager &) = delete;
  DagBlockManager(DagBlockManager &&) = delete;
  DagBlockManager &operator=(const DagBlockManager &) = delete;
  DagBlockManager &operator=(DagBlockManager &&) = delete;

  InsertAndVerifyBlockReturnType insertAndVerifyBlock(DagBlock &&blk);
  std::optional<DagBlock> popVerifiedBlock(bool level_limit = false,
                                           uint64_t level = 0);  // get one verified block and pop
  void pushVerifiedBlock(const DagBlock &blk);
  size_t getDagBlockQueueSize() const;
  level_t getMaxDagLevelInQueue() const;
  void stop();

  /**
   * @param hash
   * @return true in case block was already seen or is part of dag structure
   */
  bool isDagBlockKnown(const blk_hash_t &hash);

  /**
   * @brief Mark block as seen
   *
   * @param dag_block
   * @return true in case block was actually marked as seen(was not seen before), otherwise false (was already seen)
   */
  bool markDagBlockAsSeen(const DagBlock &dag_block);

  /**
   * @brief Sets dag expiry level
   *
   * @param dag_expiry_level
   */
  void setDagExpiryLevel(uint64_t dag_expiry_level) { dag_expiry_level_ = dag_expiry_level; }

  /**
   * @brief Retrieves Dag expiry level
   *
   * @return level
   */
  uint64_t getDagExpiryLevel() { return dag_expiry_level_; }

  std::shared_ptr<DagBlock> getDagBlock(const blk_hash_t &hash) const;
  bool pivotAndTipsValid(const DagBlock &blk);

  SortitionParamsManager &sortitionParamsManager() { return sortition_params_manager_; }

  const DagConfig &getDagConfig() const { return dag_config_; }

 private:
  using uLock = std::unique_lock<std::shared_mutex>;
  using sharedLock = std::shared_lock<std::shared_mutex>;

  InsertAndVerifyBlockReturnType verifyBlock(const DagBlock &blk);
  void markBlockInvalid(const blk_hash_t &hash);

  const uint32_t cache_max_size_ = 10000;
  const uint32_t cache_delete_step_ = 100;
  std::atomic<bool> stopped_ = false;

  std::shared_ptr<DbStorage> db_;
  std::shared_ptr<TransactionManager> trx_mgr_;
  std::shared_ptr<FinalChain> final_chain_;
  std::shared_ptr<PbftChain> pbft_chain_;
  logger::Logger log_time_;

  ExpirationCache<blk_hash_t> invalid_blocks_;
  ExpirationCacheMap<blk_hash_t, DagBlock> seen_blocks_;

  const uint32_t queue_limit_;
  mutable std::shared_mutex shared_mutex_for_verified_qu_;
  std::condition_variable_any cond_for_verified_qu_;
  std::map<uint64_t, std::deque<DagBlock>> verified_qu_;

  std::atomic_uint64_t dag_expiry_level_ =
      0;  // Level below which dag blocks are considered expired, it's value is
          // always current anchor level minus dag_expiry_limit_ of non empty pbft periods

  SortitionParamsManager sortition_params_manager_;
  const DagConfig dag_config_;

  LOG_OBJECTS_DEFINE
};
/** @}*/

}  // namespace taraxa
