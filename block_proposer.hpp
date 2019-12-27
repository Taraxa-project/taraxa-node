#ifndef TARAXA_NODE_BLOCK_PROPOSER_HPP
#define TARAXA_NODE_BLOCK_PROPOSER_HPP

#include <libdevcore/Log.h>
#include <atomic>
#include <random>
#include <thread>
#include <vector>
#include "boost/thread.hpp"
#include "config.hpp"
#include "dag_block.hpp"
#include "vdf_sortition.hpp"

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
  void setProposer(std::shared_ptr<BlockProposer> proposer, secret_t const& sk,
                   vrf_sk_t const& vrf_sk) {
    proposer_ = proposer;
    sk_ = sk;
    vrf_sk_ = vrf_sk;
  }

 protected:
  std::weak_ptr<BlockProposer> proposer_;
  secret_t sk_;
  vrf_sk_t vrf_sk_;
};

class RandomPropose : public ProposeModelFace {
 public:
  RandomPropose(int min, int max) : distribution_(min, max) {
    LOG(log_nf_) << "Set random block propose threshold " << min << " ~ " << max
                 << " milli seconds.";
  }
  ~RandomPropose(){};
  bool propose() override;

 private:
  std::uniform_int_distribution<std::mt19937::result_type> distribution_;
  static std::mt19937 generator;
  dev::Logger log_si_{
      dev::createLogger(dev::Verbosity::VerbositySilent, "PR_MDL")};
  dev::Logger log_er_{
      dev::createLogger(dev::Verbosity::VerbosityError, "PR_MDL")};
  dev::Logger log_wr_{
      dev::createLogger(dev::Verbosity::VerbosityWarning, "PR_MDL")};
  dev::Logger log_nf_{
      dev::createLogger(dev::Verbosity::VerbosityInfo, "PR_MDL")};
  dev::Logger log_dg_{
      dev::createLogger(dev::Verbosity::VerbosityDebug, "PR_MDL")};
  dev::Logger log_tr_{
      dev::createLogger(dev::Verbosity::VerbosityTrace, "PR_MDL")};
};

/**
 * sortition = sign({anchor_blk_hash, level}, secret_key) < threshold
 */

class SortitionPropose : public ProposeModelFace {
 public:
  SortitionPropose(uint difficulty_bound, uint lambda_bits)
      : difficulty_bound_(difficulty_bound), lambda_bits_(lambda_bits) {
    LOG(log_nf_) << "Set sorition block propose difficulty " << difficulty_bound
                 << " lambda_bits " << lambda_bits;
  }
  ~SortitionPropose() {}
  bool propose() override;

 private:
  inline static uint min_propose_delay = 200;
  uint difficulty_bound_;
  uint lambda_bits_;
  unsigned long long last_proposed_level_ = 0;
  dev::Logger log_si_{
      dev::createLogger(dev::Verbosity::VerbositySilent, "PR_MDL")};
  dev::Logger log_er_{
      dev::createLogger(dev::Verbosity::VerbosityError, "PR_MDL")};
  dev::Logger log_wr_{
      dev::createLogger(dev::Verbosity::VerbosityWarning, "PR_MDL")};
  dev::Logger log_nf_{
      dev::createLogger(dev::Verbosity::VerbosityInfo, "PR_MDL")};
  dev::Logger log_dg_{
      dev::createLogger(dev::Verbosity::VerbosityDebug, "PR_MDL")};
  dev::Logger log_tr_{
      dev::createLogger(dev::Verbosity::VerbosityTrace, "PR_MDL")};
};

/**
 * Single thread
 * Block proproser request for unpacked transaction
 */

class BlockProposer : public std::enable_shared_from_this<BlockProposer> {
 public:
  BlockProposer(std::vector<uint> const& params,
                std::shared_ptr<DagManager> dag_mgr,
                std::shared_ptr<TransactionManager> trx_mgr)
      : dag_mgr_(dag_mgr), trx_mgr_(trx_mgr) {
    // configure
    conf_.mode = params[0];
    conf_.shard = params[1];
    conf_.params.push_back(params[2]);
    conf_.params.push_back(params[3]);

    if (conf_.mode == 0) {
      uint min_freq = conf_.params[0];
      uint max_freq = conf_.params[1];
      propose_model_ = std::make_unique<RandomPropose>(min_freq, max_freq);
    } else if (conf_.mode == 1) {
      uint difficulty_bound = conf_.params[0];
      uint lambda_bits = conf_.params[1];
      propose_model_ =
          std::make_unique<SortitionPropose>(difficulty_bound, lambda_bits);
    }
    total_trx_shards_ = std::max(conf_.shard, 1u);

    // setup shards
  }

  ~BlockProposer() { stop(); }

  void setFullNode(std::shared_ptr<FullNode> full_node);
  void start();
  void stop();
  std::shared_ptr<BlockProposer> getShared();
  void proposeBlock(DagBlock& blk);
  bool getShardedTrxs(vec_trx_t& sharded_trx, DagFrontier& frontier) {
    return getShardedTrxs(total_trx_shards_, frontier, my_trx_shard_,
                          sharded_trx);
  }
  bool getLatestPivotAndTips(blk_hash_t& pivot, vec_blk_t& tips);
  level_t getProposeLevel(blk_hash_t const& pivot, vec_blk_t const& tips);
  level_t getMaxDagLevel() const;
  blk_hash_t getLatestAnchor() const;
  // debug
  static uint64_t getNumProposedBlocks() {
    return BlockProposer::num_proposed_blocks;
  }
  friend ProposeModelFace;

 private:
  struct Conf {
    uint mode;
    uint shard;
    std::vector<uint> params;
  };
  bool getShardedTrxs(uint total_shard, DagFrontier& frontier, uint my_shard,
                      vec_trx_t& sharded_trx);
  addr_t getFullNodeAddress() const;

  static std::atomic<uint64_t> num_proposed_blocks;
  std::atomic<bool> stopped_ = true;
  Conf conf_;
  uint total_trx_shards_;
  uint my_trx_shard_;
  std::weak_ptr<DagManager> dag_mgr_;
  std::weak_ptr<TransactionManager> trx_mgr_;
  std::weak_ptr<FullNode> full_node_;
  std::shared_ptr<std::thread> proposer_worker_;
  std::unique_ptr<ProposeModelFace> propose_model_;
  dev::Logger log_si_{
      dev::createLogger(dev::Verbosity::VerbositySilent, "BLK_PP")};
  dev::Logger log_er_{
      dev::createLogger(dev::Verbosity::VerbosityError, "BLK_PP")};
  dev::Logger log_wr_{
      dev::createLogger(dev::Verbosity::VerbosityWarning, "BLK_PP")};
  dev::Logger log_nf_{
      dev::createLogger(dev::Verbosity::VerbosityInfo, "BLK_PP")};
  dev::Logger log_dg_{
      dev::createLogger(dev::Verbosity::VerbosityDebug, "BLK_PP")};
  dev::Logger log_tr_{
      dev::createLogger(dev::Verbosity::VerbosityTrace, "BLK_PP")};
};

}  // namespace taraxa

#endif
