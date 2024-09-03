#pragma once

#include "dag.hpp"
#include "dag/dag_block.hpp"
#include "pbft/pbft_chain.hpp"
#include "sortition_params_manager.hpp"
#include "storage/storage.hpp"
#include "transaction/transaction_manager.hpp"

namespace taraxa {

/** @addtogroup DAG
 * @{
 */
class Network;
class DagBuffer;
class FullNode;
class KeyManager;
struct DagConfig;

/**
 * @brief DagManager class contains in memory representation of part of the DAG that is not yet finalized in a pbft
 * block and validates DAG blocks
 *
 * DagManager validates incoming dag blocks and provides ordering of non-finalized DAG block with getDagOrder method.
 * Once a pbft block is finalized setDagBlockOrder is invoked which removes all the finalized DAG blocks from in memory
 * DAG. DagManager class is thread safe in general with exception of function setDagBlockOrder. See details in function
 * descriptions.
 */
class DagManager : public std::enable_shared_from_this<DagManager> {
 public:
  /**
   * @brief return type of verifyBlock
   */
  enum class VerifyBlockReturnType : uint32_t {
    Verified = 0,
    MissingTransaction,
    AheadBlock,
    FailedVdfVerification,
    FutureBlock,
    NotEligible,
    ExpiredBlock,
    IncorrectTransactionsEstimation,
    BlockTooBig,
    FailedTipsVerification,
    MissingTip,
    InvalidSender,
  };

  explicit DagManager(const FullNodeConfig &config, addr_t node_addr, std::shared_ptr<TransactionManager> trx_mgr,
                      std::shared_ptr<PbftChain> pbft_chain, std::shared_ptr<final_chain::FinalChain> final_chain,
                      std::shared_ptr<DbStorage> db, std::shared_ptr<KeyManager> key_manager);

  DagManager(const DagManager &) = delete;
  DagManager(DagManager &&) = delete;
  DagManager &operator=(const DagManager &) = delete;
  DagManager &operator=(DagManager &&) = delete;

  std::shared_ptr<DagManager> getShared();

  void setNetwork(std::weak_ptr<Network> network) { network_ = std::move(network); }

  /**
   * @param hash
   * @return true in case block was already seen or is part of dag structure
   */
  bool isDagBlockKnown(const blk_hash_t &hash) const;

  /**
   * @brief Gets dag block from either local memory cache or db
   * @param hash Block hash
   * @return Block or nullptr if block is not found
   */
  std::shared_ptr<DagBlock> getDagBlock(const blk_hash_t &hash) const;

  /**
   * @brief Verifies new DAG block
   * @param blk Block to verify
   * @param transactions Optional block transactions
   * @return verification result and all the transactions which are part of the block
   */
  std::pair<VerifyBlockReturnType, SharedTransactions> verifyBlock(
      const DagBlock &blk, const std::unordered_map<trx_hash_t, std::shared_ptr<Transaction>> &trxs = {});

  /**
   * @brief Checks if block pivot and tips are in DAG
   * @param blk Block to check
   * @return true if all pivot and tips are in the DAG, false if some is missing with the hash of missing tips/pivot
   */
  std::pair<bool, std::vector<blk_hash_t>> pivotAndTipsAvailable(DagBlock const &blk);

  /**
   * @brief adds verified DAG block in the DAG
   * @param trxs Block transactions
   * @param proposed if this node proposed the block
   * @param save if true save block and transactions to database
   * @return true if block added successfully, false with the hash of missing tips/pivot
   */
  std::pair<bool, std::vector<blk_hash_t>> addDagBlock(DagBlock &&blk, SharedTransactions &&trxs = {},
                                                       bool proposed = false,
                                                       bool save = true);  // insert to buffer if fail

  /**
   * @brief Retrieves DAG block order for specified anchor. Order should always be the same for the same anchor in the
   * same period
   * @param anchor anchor block
   * @param period period
   * @return ordered blocks
   */
  vec_blk_t getDagBlockOrder(blk_hash_t const &anchor, PbftPeriod period);

  /**
   * @brief Sets the dag block order on finalizing PBFT block
   * IMPORTANT: This method is invoked on finalizing a pbft block, it needs to be protected with mutex_ but the mutex is
   * locked from pbft manager for the entire pbft finalization to make the finalization atomic
   *
   * @param anchor Anchor of the finalized pbft block
   * @param period Period finalized
   * @param dag_order Dag order of the finalized pbft block
   *
   * @return number of dag blocks finalized
   */
  uint setDagBlockOrder(blk_hash_t const &anchor, PbftPeriod period, vec_blk_t const &dag_order);

  std::optional<std::pair<blk_hash_t, std::vector<blk_hash_t>>> getLatestPivotAndTips() const;

  /**
   * @brief Retrieves ghost path which is ordered list of non finalized pivot blocks for a specific source block
   * @param source source block
   * @return returned ordered blocks
   */
  std::vector<blk_hash_t> getGhostPath(const blk_hash_t &source) const;

  /**
   * @brief Retrieves ghost path which is ordered list of non finalized pivot blocks for last anchor
   * @return returned ordered blocks
   */
  std::vector<blk_hash_t> getGhostPath() const;  // get ghost path from last anchor

  // ----- Total graph
  void drawTotalGraph(std::string const &str) const;

  // ----- Pivot graph
  // can return self as pivot chain
  void drawPivotGraph(std::string const &str) const;
  void drawGraph(std::string const &dotfile) const;

  std::pair<uint64_t, uint64_t> getNumVerticesInDag() const;
  std::pair<uint64_t, uint64_t> getNumEdgesInDag() const;
  level_t getMaxLevel() const { return max_level_; }

  // DAG anchors
  PbftPeriod getLatestPeriod() const {
    std::shared_lock lock(mutex_);
    return period_;
  }
  std::pair<blk_hash_t, blk_hash_t> getAnchors() const {
    std::shared_lock lock(mutex_);
    return std::make_pair(old_anchor_, anchor_);
  }

  /**
   * @brief Retrieves Dag expiry limit
   *
   * @return limit
   */
  uint32_t getDagExpiryLimit() const { return dag_expiry_limit_; }

  const std::pair<PbftPeriod, std::map<uint64_t, std::unordered_set<blk_hash_t>>> getNonFinalizedBlocks() const;

  /**
   * @brief Retrieves current period together with non finalized blocks with the unique list of non finalized
   * transactions from these blocks
   *
   * @param known_hashes excludes known hashes from result
   * @return Period, blocks and transactions
   */
  const std::tuple<PbftPeriod, std::vector<std::shared_ptr<DagBlock>>, SharedTransactions>
  getNonFinalizedBlocksWithTransactions(const std::unordered_set<blk_hash_t> &known_hashes) const;

  DagFrontier getDagFrontier();

  /**
   * @return std::pair<size_t, size_t> -> first = number of levels, second = number of blocks
   */
  std::pair<size_t, size_t> getNonFinalizedBlocksSize() const;

  util::Event<DagManager, DagBlock> const block_verified_{};

  /**
   * @brief Retrieves Dag Manager mutex, only to be used when finalizing pbft block
   *
   * @return mutex
   */
  std::shared_mutex &getDagMutex() { return mutex_; }

  /**
   * @brief Retrieves sortition manager
   *
   * @return sortition manager
   */
  SortitionParamsManager &sortitionParamsManager() { return sortition_params_manager_; }

  /**
   * @brief Retrieves Dag config
   *
   * @return dag config
   */
  const DagConfig &getDagConfig() const { return dag_config_; }

  /**
   * @brief Retrieves Dag expiry level
   *
   * @return level
   */
  uint64_t getDagExpiryLevel() { return dag_expiry_level_; }

  /**
   * @brief Retrieves VDF message from block hash and transactions
   *
   * @return message
   */
  static dev::bytes getVdfMessage(blk_hash_t const &hash, SharedTransactions const &trxs);

  /**
   * @brief Retrieves VDF message from block hash and transactions
   *
   * @return message
   */
  static dev::bytes getVdfMessage(blk_hash_t const &hash, std::vector<trx_hash_t> const &trx_hashes);

  /**
   * @brief Clears light node history
   *
   */
  void clearLightNodeHistory(uint64_t light_node_history);

 private:
  void recoverDag();
  void addToDag(blk_hash_t const &hash, blk_hash_t const &pivot, std::vector<blk_hash_t> const &tips, uint64_t level,
                bool finalized = false);
  bool validateBlockNotExpired(const std::shared_ptr<DagBlock> &dag_block,
                               std::unordered_map<blk_hash_t, std::shared_ptr<DagBlock>> &expired_dag_blocks_to_remove);
  void handleExpiredDagBlocksTransactions(const std::vector<trx_hash_t> &transactions_from_expired_dag_blocks) const;

  std::pair<blk_hash_t, std::vector<blk_hash_t>> getFrontier() const;  // return pivot and tips
  void updateFrontier();
  std::atomic<level_t> max_level_ = 0;
  mutable std::shared_mutex mutex_;
  mutable std::shared_mutex order_dag_blocks_mutex_;
  std::shared_ptr<PivotTree> pivot_tree_;  // only contains pivot edges
  std::shared_ptr<Dag> total_dag_;         // contains both pivot and tips
  std::shared_ptr<TransactionManager> trx_mgr_;
  std::shared_ptr<PbftChain> pbft_chain_;
  std::weak_ptr<Network> network_;
  std::shared_ptr<DbStorage> db_;
  std::shared_ptr<KeyManager> key_manager_;
  blk_hash_t anchor_;      // anchor of the last period
  blk_hash_t old_anchor_;  // anchor of the second to last period
  PbftPeriod period_;      // last period
  std::map<uint64_t, std::unordered_set<blk_hash_t>> non_finalized_blks_;
  DagFrontier frontier_;
  SortitionParamsManager sortition_params_manager_;
  const DagConfig &dag_config_;
  const std::shared_ptr<DagBlock> genesis_block_;
  const uint32_t max_levels_per_period_;
  const uint32_t dag_expiry_limit_;  // Any non finalized dag block with a level smaller by
  // dag_expiry_limit_ than the current period anchor level is considered
  // expired and it should be ignored or removed from DAG

  std::atomic_uint64_t dag_expiry_level_ =
      0;  // Level below which dag blocks are considered expired, it's value is
          // always current anchor level minus dag_expiry_limit_ of non empty pbft periods

  const uint32_t cache_max_size_ = 10000;
  const uint32_t cache_delete_step_ = 100;
  ExpirationCacheMap<blk_hash_t, DagBlock> seen_blocks_;
  std::shared_ptr<final_chain::FinalChain> final_chain_;
  const uint64_t kPbftGasLimit;
  const HardforksConfig kHardforks;
  const uint64_t kValidatorMaxVote;

  LOG_OBJECTS_DEFINE
};

/** @}*/

}  // namespace taraxa
