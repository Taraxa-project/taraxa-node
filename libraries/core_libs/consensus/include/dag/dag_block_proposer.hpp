#pragma once

#include <atomic>
#include <random>
#include <thread>
#include <vector>

#include "boost/thread.hpp"
#include "config/config.hpp"
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
  DagBlockProposer(const DagBlockProposerConfig& bp_config, std::shared_ptr<DagManager> dag_mgr,
                   std::shared_ptr<TransactionManager> trx_mgr, std::shared_ptr<final_chain::FinalChain> final_chain,
                   std::shared_ptr<DbStorage> db, std::shared_ptr<KeyManager> key_manager, addr_t node_addr,
                   secret_t node_sk, vrf_wrapper::vrf_sk_t vrf_sk);
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
   *
   * @return true if successfully proposed, otherwise false
   */
  bool proposeDagBlock();

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

 private:
  /**
   * @brief Creates a new block with provided data
   * @param frontier frontier to use for pivot and tips of the new block
   * @param level level of the new block
   * @param trxs transactions to be included in the block
   * @param estimations transactions gas estimation
   * @param vdf vdf with correct difficulty calculation
   */
  DagBlock createDagBlock(DagFrontier&& frontier, level_t level, const SharedTransactions& trxs,
                          std::vector<uint64_t>&& estimations, VdfSortition&& vdf) const;

  /**
   * @brief Gets transactions to include in the block - sharding not supported yet
   * @param proposal_period proposal period
   * @param weight_limit weight limit
   * @return transactions and weight estimations
   */
  std::pair<SharedTransactions, std::vector<uint64_t>> getShardedTrxs(PbftPeriod proposal_period,
                                                                      uint64_t weight_limit) const;

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
   * @return true if eligible to propose
   */
  bool isValidDposProposer(PbftPeriod propose_period) const;

 private:
  uint16_t max_num_tries_{20};  // Wait 2000(ms)
  uint16_t num_tries_{0};
  uint64_t last_propose_level_{0};
  util::ThreadPool executor_{1};

  std::atomic<uint64_t> proposed_blocks_count_{0};
  std::atomic<bool> stopped_{true};

  const DagBlockProposerConfig bp_config_;
  const uint16_t total_trx_shards_;
  uint16_t my_trx_shard_;

  std::shared_ptr<DagManager> dag_mgr_;
  std::shared_ptr<TransactionManager> trx_mgr_;
  std::shared_ptr<final_chain::FinalChain> final_chain_;
  std::shared_ptr<KeyManager> key_manager_;
  std::shared_ptr<DbStorage> db_;
  std::shared_ptr<std::thread> proposer_worker_;
  std::weak_ptr<Network> network_;

  const addr_t node_addr_;
  const secret_t node_sk_;
  const vrf_wrapper::vrf_sk_t vrf_sk_;
  const vrf_wrapper::vrf_pk_t vrf_pk_;

  LOG_OBJECTS_DEFINE
};

/**
 * @}
 */

}  // namespace taraxa
