#pragma once

#include <atomic>
#include <thread>
#include <vector>

#include "dag/dag_block.hpp"
#include "logger/logger.hpp"
#include "network/network.hpp"

namespace taraxa {

/** @addtogroup DAG
 * @{
 */
class TransactionManager;
class KeyManager;
class DagManager;
struct FullNodeConfig;

namespace final_chain {
class FinalChain;
}

/**
 * @brief DagBlockProposer class proposes new DAG blocks using transactions retrieved from TransactionManager
 *
 * Class is running a proposer thread which will try to propose DAG blocks if eligible to propose.
 * Dag block proposal consists of calculating VDF if required.
 * VDF calculation is asynchronous and it is interrupted if another node on the network has produced a valid block at
 * the same level. Proposal includes stale block proposal in case no block is produced on the network for extended
 * period of time.
 */
class DagBlockProposer {
 public:
  struct NodeDagProposerData {
    NodeDagProposerData(const WalletConfig& wallet, const uint16_t max_tries, const uint16_t shard)
        : wallet(wallet),
          max_num_tries(max_tries + (wallet.node_addr[0] % (10 * max_tries))),
          trx_shard(std::stoull(wallet.node_addr.toString().substr(0, 6).c_str(), NULL, 16) % shard) {}

    const WalletConfig wallet;

    // Add a random component in proposing stale blocks so that not all nodes propose stale blocks at the same time
    // This will make stale block be proposed after waiting random interval between 2 and 20 seconds
    const uint16_t max_num_tries;
    const uint16_t trx_shard;

    uint16_t num_tries{0};
    uint64_t last_propose_level{0};
  };

 public:
  DagBlockProposer(const FullNodeConfig& config, std::shared_ptr<DagManager> dag_mgr,
                   std::shared_ptr<TransactionManager> trx_mgr, std::shared_ptr<final_chain::FinalChain> final_chain,
                   std::shared_ptr<DbStorage> db, std::shared_ptr<KeyManager> key_manager);
  ~DagBlockProposer() { stop(); }
  DagBlockProposer(const DagBlockProposer&) = delete;
  DagBlockProposer(DagBlockProposer&&) = delete;
  DagBlockProposer& operator=(const DagBlockProposer&) = delete;
  DagBlockProposer& operator=(DagBlockProposer&&) = delete;

  /**
   * @brief Start the worker thread proposing new DAG blocks
   */
  void start();

  /**
   * @brief Stop the worker thread
   */
  void stop();

  /**
   * @brief Tries to propose new dag block
   * @param node_dag_proposer_data data for the node proposing the block
   *
   * @return true if successfully proposed, otherwise false
   */
  bool proposeDagBlock(const std::shared_ptr<NodeDagProposerData>& node_dag_proposer_data);

  /**
   * @brief Sets network
   *
   * @param network
   */
  void setNetwork(std::weak_ptr<Network> network);

  /**
   * @return number of proposed dag blocks since the last start
   */
  uint64_t getProposedBlocksCount() const { return proposed_blocks_count_; }

  /**
   * @brief Select tips for DagBlock proposal up to max allowed
   *
   * @param frontier_tips
   * @param gas_limit gas limit for the tips
   *
   * @return tips
   */
  vec_blk_t selectDagBlockTips(const vec_blk_t& frontier_tips, uint64_t gas_limit) const;

 private:
  /**
   * @brief Creates a new block with provided data
   * @param frontier frontier to use for pivot and tips of the new block
   * @param level level of the new block
   * @param trxs transactions to be included in the block
   * @param estimations transactions gas estimation
   * @param vdf vdf with correct difficulty calculation
   * @param node_secret
   */
  std::shared_ptr<DagBlock> createDagBlock(DagFrontier&& frontier, level_t level, const SharedTransactions& trxs,
                                           std::vector<uint64_t>&& estimations, VdfSortition&& vdf,
                                           const dev::Secret& node_secret) const;

  /**
   * @brief Gets transactions to include in the block - sharding not supported yet
   * @param proposal_period proposal period
   * @param weight_limit weight limit
   * @param node_trx_shard
   * @return transactions and weight estimations
   */
  std::pair<SharedTransactions, std::vector<uint64_t>> getShardedTrxs(PbftPeriod proposal_period, uint64_t weight_limit,
                                                                      const uint16_t node_trx_shard) const;

  /**
   * @brief Gets current propose level for provided pivot and tips
   * @param pivot pivot block hash
   * @param tips tips blocks hashes
   * @return level
   */
  level_t getProposeLevel(blk_hash_t const& pivot, vec_blk_t const& tips) const;

  /**
   * @brief Checks if node is valid proposer for provided level
   * @param level level of the new block
   * @param node_addr
   * @return true if eligible to propose
   */
  bool isValidDposProposer(PbftPeriod propose_period, const addr_t& node_addr) const;

 private:
  const uint16_t max_num_tries_{20};  // Wait 2000(ms)
                                      //  uint16_t num_tries_{0};
                                      //  uint64_t last_propose_level_{0};
  util::ThreadPool executor_{1};

  std::atomic<uint64_t> proposed_blocks_count_{0};
  std::atomic<bool> stopped_{true};

  const uint16_t total_trx_shards_;

  std::shared_ptr<DagManager> dag_mgr_;
  std::shared_ptr<TransactionManager> trx_mgr_;
  std::shared_ptr<final_chain::FinalChain> final_chain_;
  std::shared_ptr<KeyManager> key_manager_;
  std::shared_ptr<DbStorage> db_;
  std::vector<std::thread> proposer_workers_;
  std::weak_ptr<Network> network_;

  std::vector<std::shared_ptr<NodeDagProposerData>> nodes_dag_proposers_data_;

  const uint64_t kDagProposeGasLimit;
  const uint64_t kPbftGasLimit;
  const uint64_t kDagGasLimit;

  const HardforksConfig kHardforks;
  const uint64_t kValidatorMaxVote;
  const uint64_t kShardProposePeriodInterval = 10;

  LOG_OBJECTS_DEFINE
};

/**
 * @}
 */

}  // namespace taraxa
