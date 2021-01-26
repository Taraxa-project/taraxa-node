#ifndef TARAXA_NODE_BLOCK_PROPOSER_HPP
#define TARAXA_NODE_BLOCK_PROPOSER_HPP

#include <atomic>
#include <random>
#include <thread>
#include <vector>

#include "boost/thread.hpp"
#include "config/config.hpp"
#include "dag/dag_block_manager.hpp"
#include "dag/vdf_sortition.hpp"
#include "logger/log.hpp"
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
  virtual ~ProposeModelFace() {}
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
  SortitionPropose(vdf_sortition::VdfConfig const& vdf_config, addr_t node_addr, std::shared_ptr<DagManager> dag_mgr,
                   std::shared_ptr<TransactionManager> trx_mgr)
      : vdf_config_(vdf_config), dag_mgr_(dag_mgr), trx_mgr_(trx_mgr) {
    LOG_OBJECTS_CREATE("PR_MDL");
    LOG(log_nf_) << "Set sorition DAG block proposal" << vdf_config_;
  }
  ~SortitionPropose() {}
  bool propose() override;

 private:
  vdf_sortition::VdfConfig vdf_config_;
  int num_tries_ = 0;
  const int max_num_tries_ = 20;  // Wait 2000(ms)
  level_t last_propose_level_ = 0;
  std::shared_ptr<DagManager> dag_mgr_;
  std::shared_ptr<TransactionManager> trx_mgr_;

  LOG_OBJECTS_DEFINE;
};

/**
 * Single thread
 * Block proproser request for unpacked transaction
 */
class BlockProposer : public std::enable_shared_from_this<BlockProposer> {
 public:
  BlockProposer(BlockProposerConfig const& bp_config, vdf_sortition::VdfConfig const& vdf_config,
                std::shared_ptr<DagManager> dag_mgr, std::shared_ptr<TransactionManager> trx_mgr,
                std::shared_ptr<DagBlockManager> dag_blk_mgr, std::shared_ptr<FinalChain> final_chain, addr_t node_addr,
                secret_t node_sk, vrf_sk_t vrf_sk, logger::Logger log_time)
      : dag_mgr_(dag_mgr),
        trx_mgr_(trx_mgr),
        dag_blk_mgr_(dag_blk_mgr),
        final_chain_(final_chain),
        bp_config_(bp_config),
        log_time_(log_time),
        node_addr_(node_addr),
        node_sk_(node_sk),
        vrf_sk_(vrf_sk) {
    LOG_OBJECTS_CREATE("PR_MDL");
    propose_model_ = std::make_unique<SortitionPropose>(vdf_config, node_addr, dag_mgr, trx_mgr);
    total_trx_shards_ = std::max((unsigned int)bp_config_.shard, 1u);
    auto addr = std::stoull(node_addr.toString().substr(0, 6).c_str(), NULL, 16);
    my_trx_shard_ = addr % bp_config_.shard;
    LOG(log_nf_) << "Block proposer in " << my_trx_shard_ << " shard ...";
  }

  ~BlockProposer() { stop(); }

  void start();
  void stop();
  std::shared_ptr<BlockProposer> getShared();
  void setNetwork(std::shared_ptr<Network> network) { network_ = network; }
  void proposeBlock(DagBlock& blk);
  bool getShardedTrxs(vec_trx_t& sharded_trx) { return getShardedTrxs(total_trx_shards_, my_trx_shard_, sharded_trx); }
  bool getLatestPivotAndTips(blk_hash_t& pivot, vec_blk_t& tips);
  level_t getProposeLevel(blk_hash_t const& pivot, vec_blk_t const& tips);
  blk_hash_t getProposeAnchor() const;
  bool validDposProposer(level_t const propose_level);
  // debug
  static uint64_t getNumProposedBlocks() { return BlockProposer::num_proposed_blocks; }
  friend ProposeModelFace;

 private:
  bool getShardedTrxs(uint total_shard, uint my_shard, vec_trx_t& sharded_trx);
  addr_t getFullNodeAddress() const;

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
  std::shared_ptr<std::thread> proposer_worker_;
  std::unique_ptr<ProposeModelFace> propose_model_;
  std::shared_ptr<Network> network_;
  logger::Logger log_time_;
  addr_t node_addr_;
  secret_t node_sk_;
  vrf_sk_t vrf_sk_;
  LOG_OBJECTS_DEFINE;
};

}  // namespace taraxa

#endif
