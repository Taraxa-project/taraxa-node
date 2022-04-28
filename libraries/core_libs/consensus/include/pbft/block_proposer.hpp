#pragma once

#include <atomic>
#include <random>
#include <thread>
#include <vector>

#include "boost/thread.hpp"
#include "config/config.hpp"
#include "logger/logger.hpp"
#include "network/network.hpp"

namespace taraxa {

class DagManager;
class TransactionManager;
class FullNode;
class BlockProposer;
class DagBlock;

using vrf_sk_t = vrf_wrapper::vrf_sk_t;

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
                   vrf_sk_t const& vrf_sk) {
    proposer_ = proposer;
    node_addr_ = node_addr;
    sk_ = sk;
    vrf_sk_ = vrf_sk;
  }

 protected:
  std::weak_ptr<BlockProposer> proposer_;
  addr_t node_addr_;
  secret_t sk_;
  vrf_sk_t vrf_sk_;
};

class SortitionPropose : public ProposeModelFace {
 public:
  SortitionPropose(addr_t node_addr, std::shared_ptr<DbStorage> db, std::shared_ptr<DagManager> dag_mgr,
                   std::shared_ptr<DagBlockManager> dag_blk_mgr, std::shared_ptr<TransactionManager> trx_mgr)
      : db_(std::move(db)),
        dag_mgr_(std::move(dag_mgr)),
        dag_blk_mgr_(std::move(dag_blk_mgr)),
        trx_mgr_(std::move(trx_mgr)) {
    LOG_OBJECTS_CREATE("PR_MDL");
    LOG(log_nf_) << "Set sortition DAG block proposal" << dag_blk_mgr_->sortitionParamsManager().getSortitionParams();
    // Add a random component in proposing stale blocks so that not all nodes propose stale blocks at the same time
    // This will make stale block be proposed after waiting random interval between 2 and 20 seconds
    max_num_tries_ += (node_addr_[0] % (10 * max_num_tries_));
  }

  bool propose() override;

 private:
  int num_tries_ = 0;
  int max_num_tries_ = 20;  // Wait 2000(ms)
  DagFrontier last_frontier_;
  std::shared_ptr<DbStorage> db_;
  std::shared_ptr<DagManager> dag_mgr_;
  std::shared_ptr<DagBlockManager> dag_blk_mgr_;
  std::shared_ptr<TransactionManager> trx_mgr_;

  LOG_OBJECTS_DEFINE
};

/**
 * Single thread
 * Block proproser request for unpacked transaction
 */
class BlockProposer : public std::enable_shared_from_this<BlockProposer> {
 public:
  BlockProposer(BlockProposerConfig const& bp_config, std::shared_ptr<DagManager> dag_mgr,
                std::shared_ptr<TransactionManager> trx_mgr, std::shared_ptr<DagBlockManager> dag_blk_mgr,
                std::shared_ptr<FinalChain> final_chain, std::shared_ptr<DbStorage> db, addr_t node_addr,
                secret_t node_sk, vrf_sk_t vrf_sk)
      : bp_config_(bp_config),
        dag_mgr_(std::move(dag_mgr)),
        trx_mgr_(std::move(trx_mgr)),
        dag_blk_mgr_(std::move(dag_blk_mgr)),
        final_chain_(std::move(final_chain)),
        db_(std::move(db)),
        node_addr_(node_addr),
        node_sk_(node_sk),
        vrf_sk_(vrf_sk) {
    LOG_OBJECTS_CREATE("PR_MDL");
    propose_model_ = std::make_unique<SortitionPropose>(node_addr, db_, dag_mgr_, dag_blk_mgr_, trx_mgr_);
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

  void start();
  void stop();
  std::shared_ptr<BlockProposer> getShared();
  void setNetwork(std::weak_ptr<Network> network) { network_ = move(network); }
  void proposeBlock(DagFrontier&& frontier, level_t level, std::optional<uint64_t> proposal_period,
                    SharedTransactions&& trxs, VdfSortition&& vdf);
  SharedTransactions getShardedTrxs();
  level_t getProposeLevel(blk_hash_t const& pivot, vec_blk_t const& tips);
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
  std::shared_ptr<DagBlockManager> dag_blk_mgr_;
  std::shared_ptr<FinalChain> final_chain_;
  std::shared_ptr<DbStorage> db_;
  std::shared_ptr<std::thread> proposer_worker_;
  std::unique_ptr<ProposeModelFace> propose_model_;
  std::weak_ptr<Network> network_;
  addr_t node_addr_;
  secret_t node_sk_;
  vrf_sk_t vrf_sk_;
  LOG_OBJECTS_DEFINE
};

}  // namespace taraxa
