#pragma once

#include <atomic>
#include <random>
#include <thread>
#include <vector>

#include "boost/thread.hpp"
#include "config/config.hpp"
#include "dag/dag_manager.hpp"
#include "logger/logger.hpp"
#include "network/network.hpp"

namespace taraxa {

/** @addtogroup DAG
 * @{
 */

class TransactionManager;
class FullNode;
class BlockProposer;
class DagBlock;
class KeyManager;

class ProposeModelFace {
 public:
  ProposeModelFace() = default;
  virtual ~ProposeModelFace() = default;
  ProposeModelFace(const ProposeModelFace&) = default;
  ProposeModelFace(ProposeModelFace&&) = default;
  ProposeModelFace& operator=(const ProposeModelFace&) = default;
  ProposeModelFace& operator=(ProposeModelFace&&) = default;

  virtual bool propose() = 0;
  void setProposer(std::shared_ptr<BlockProposer> proposer, addr_t node_addr, secret_t const& sk,
                   vrf_wrapper::vrf_sk_t const& vrf_sk) {
    proposer_ = proposer;
    node_addr_ = node_addr;
    sk_ = sk;
    vrf_sk_ = vrf_sk;
    vrf_pk_ = vrf_wrapper::getVrfPublicKey(vrf_sk);
  }

 protected:
  std::weak_ptr<BlockProposer> proposer_;
  addr_t node_addr_;
  secret_t sk_;
  vrf_wrapper::vrf_sk_t vrf_sk_;
  vrf_wrapper::vrf_pk_t vrf_pk_;
};

class SortitionPropose : public ProposeModelFace {
 public:
  SortitionPropose(addr_t node_addr, std::shared_ptr<DbStorage> db, std::shared_ptr<DagManager> dag_mgr,
                   std::shared_ptr<TransactionManager> trx_mgr, std::shared_ptr<KeyManager> key_manager)
      : db_(std::move(db)),
        dag_mgr_(std::move(dag_mgr)),
        trx_mgr_(std::move(trx_mgr)),
        key_manager_(std::move(key_manager)) {
    LOG_OBJECTS_CREATE("PR_MDL");
    LOG(log_nf_) << "Set sortition DAG block proposal" << dag_mgr_->sortitionParamsManager().getSortitionParams();
    // Add a random component in proposing stale blocks so that not all nodes propose stale blocks at the same time
    // This will make stale block be proposed after waiting random interval between 2 and 20 seconds
    max_num_tries_ += (node_addr_[0] % (10 * max_num_tries_));
  }

  ~SortitionPropose() { executor_.stop(); }

  bool propose() override;

 private:
  int num_tries_ = 0;
  int max_num_tries_ = 20;  // Wait 2000(ms)
  uint64_t last_propose_level_ = 0;
  util::ThreadPool executor_{1};
  std::shared_ptr<DbStorage> db_;
  std::shared_ptr<DagManager> dag_mgr_;
  std::shared_ptr<TransactionManager> trx_mgr_;
  std::shared_ptr<KeyManager> key_manager_;

  LOG_OBJECTS_DEFINE
};

/**
 * @brief BlockProposer class proposes new DAG blocks using transactions retrieved from TransactionManager
 *
 * Class is running a proposer thread which will try to propose DAG blocks if eligible to propose.
 * Dag block proposal consists of calculating VDF if required.
 * VDF calculation is asynchronous and it is interrupted if another node on the network has produced a valid block at
 * the same level. Proposal includes stale block proposal in case no block is produced on the network for extended
 * period of time.
 */
class BlockProposer : public std::enable_shared_from_this<BlockProposer> {
 public:
  BlockProposer(BlockProposerConfig const& bp_config, std::shared_ptr<DagManager> dag_mgr,
                std::shared_ptr<TransactionManager> trx_mgr, std::shared_ptr<FinalChain> final_chain,
                std::shared_ptr<DbStorage> db, std::shared_ptr<KeyManager> key_manager, addr_t node_addr,
                secret_t node_sk, vrf_wrapper::vrf_sk_t vrf_sk)
      : bp_config_(bp_config),
        dag_mgr_(std::move(dag_mgr)),
        trx_mgr_(std::move(trx_mgr)),
        final_chain_(std::move(final_chain)),
        db_(std::move(db)),
        node_addr_(node_addr),
        node_sk_(node_sk),
        vrf_sk_(vrf_sk) {
    LOG_OBJECTS_CREATE("PR_MDL");
    propose_model_ = std::make_unique<SortitionPropose>(node_addr, db_, dag_mgr_, trx_mgr_, std::move(key_manager));
    total_trx_shards_ = std::max((unsigned int)bp_config_.shard, 1u);
    auto addr = std::stoull(node_addr.toString().substr(0, 6).c_str(), NULL, 16);
    my_trx_shard_ = addr % bp_config_.shard;
    LOG(log_nf_) << "Block proposer in " << my_trx_shard_ << " shard ...";
  }

  ~BlockProposer() { stop(); }
  BlockProposer(const BlockProposer&) = delete;
  BlockProposer(BlockProposer&&) = delete;
  BlockProposer& operator=(const BlockProposer&) = delete;
  BlockProposer& operator=(BlockProposer&&) = delete;

  /**
   * @brief Start the worker thread proposing new DAG blocks
   */
  void start();

  /**
   * @brief Stop the worker thread
   */
  void stop();

  std::shared_ptr<BlockProposer> getShared();
  void setNetwork(std::weak_ptr<Network> network) { network_ = std::move(network); }

  /**
   * @brief Creates/proposes a new block with provided data
   * @param frontier frontier to use for pivot and tips of the new block
   * @param level level of the new block
   * @param trxs transactions to be included in the block
   * @param estimations transactions gas estimation
   * @param vdf vdf with correct difficulty calculation
   */
  void proposeBlock(DagFrontier&& frontier, level_t level, SharedTransactions&& trxs,
                    std::vector<uint64_t>&& estimations, VdfSortition&& vdf);

  /**
   * @brief Gets transactions to include in the block - sharding not supported yet
   * @param proposal_period proposal period
   * @param weight_limit weight limit
   * @return transactions and weight estimations
   */
  std::pair<SharedTransactions, std::vector<uint64_t>> getShardedTrxs(PbftPeriod proposal_period,
                                                                      uint64_t weight_limit);

  /**
   * @brief Gets current propose level for provided pivot and tips
   * @param pivot pivot block hash
   * @param tips tips blocks hashes
   * @return level
   */
  level_t getProposeLevel(blk_hash_t const& pivot, vec_blk_t const& tips);

  /**
   * @brief Checks if valid proposer for provided level
   * @param level level of the new block
   * @return true if eligible to propose
   */
  bool validDposProposer(level_t const propose_level);

  // debug
  static uint64_t getNumProposedBlocks() { return BlockProposer::num_proposed_blocks; }
  friend ProposeModelFace;

 private:
  inline static const uint16_t min_proposal_delay = 100;
  static std::atomic<uint64_t> num_proposed_blocks;
  std::atomic<bool> stopped_ = true;
  BlockProposerConfig bp_config_;
  uint16_t total_trx_shards_;
  uint16_t my_trx_shard_;
  std::shared_ptr<DagManager> dag_mgr_;
  std::shared_ptr<TransactionManager> trx_mgr_;
  std::shared_ptr<FinalChain> final_chain_;
  std::shared_ptr<DbStorage> db_;
  std::shared_ptr<std::thread> proposer_worker_;
  std::unique_ptr<ProposeModelFace> propose_model_;
  std::weak_ptr<Network> network_;
  addr_t node_addr_;
  secret_t node_sk_;
  vrf_wrapper::vrf_sk_t vrf_sk_;
  LOG_OBJECTS_DEFINE
};

/**
 * @}
 */

}  // namespace taraxa
