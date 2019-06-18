/*
 * @Copyright: Taraxa.io
 * @Author: Chia-Chun Lin
 * @Date: 2019-01-24 11:30:32
 * @Last Modified by: Chia-Chun Lin
 * @Last Modified time: 2019-04-23 15:47:40
 */

#ifndef BLOCK_PROPOSER_HPP
#define BLOCK_PROPOSER_HPP
#include <libdevcore/Log.h>
#include <random>
#include <thread>
#include <vector>
#include "boost/thread.hpp"
#include "config.hpp"

namespace taraxa {

class DagManager;
class TransactionManager;
class FullNode;
class BlockProposer;
class DagBlock;
class ProposeModelFace {
 public:
  virtual ~ProposeModelFace() {}
  virtual bool propose() = 0;
  void setProposer(std::shared_ptr<BlockProposer> proposer,
                   secret_t const& sk) {
    proposer_ = proposer;
    sk_ = sk;
  }

 protected:
  std::weak_ptr<BlockProposer> proposer_;
  secret_t sk_;
};

class RandomPropose : public ProposeModelFace {
 public:
  RandomPropose(int min, int max) : distribution_(min, max) {}
  ~RandomPropose(){};
  bool propose() override;

 private:
  std::uniform_int_distribution<std::mt19937::result_type> distribution_;
  static std::mt19937 generator;
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
 * sortition = sign({anchor_blk_hash, level}, secret_key) >= threshold
 */

class SortitionPropose : public ProposeModelFace {
 public:
  SortitionPropose(uint interval, uint threshold)
      : propose_interval_(interval), threshold_(threshold) {
    LOG(log_nf_) << "Set block propose sorition threshold " << threshold_;
  }
  ~SortitionPropose() {}
  bool propose() override;
  bool propose(blk_hash_t const& blk, uint64_t level);

 private:
  uint propose_interval_ = 1000;
  level_t last_fail_propose_level_ = 0;
  blk_hash_t anchor_hash_;
  uint64_t threshold_;
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
  BlockProposer(ProposerConfig& conf, std::shared_ptr<DagManager> dag_mgr,
                std::shared_ptr<TransactionManager> trx_mgr)
      : conf_(conf),
        total_trx_shards_(std::max(conf.shards, 1u)),
        dag_mgr_(dag_mgr),
        trx_mgr_(trx_mgr) {
    if (conf_.mode == 0) {
      propose_model_ =
          std::make_unique<RandomPropose>(conf_.param1, conf_.param2);
    } else if (conf_.mode == 1) {
      propose_model_ =
          std::make_unique<SortitionPropose>(conf_.param1, conf_.param2);
    }
  }
  ~BlockProposer() {
    if (!stopped_) stop();
  }
  void setFullNode(std::shared_ptr<FullNode> full_node);
  void start();
  void stop();
  std::shared_ptr<BlockProposer> getShared();
  void proposeBlock(DagBlock const& blk);
  bool getShardedTrxs(vec_trx_t& sharded_trx) {
    return getShardedTrxs(total_trx_shards_, my_trx_shard_, sharded_trx);
  }
  bool getLatestPivotAndTips(std::string& pivot,
                             std::vector<std::string>& tips);
  level_t getProposeLevel(blk_hash_t const& pivot, vec_blk_t const& tips);
  // debug
  static uint64_t getNumProposedBlocks() {
    return BlockProposer::num_proposed_blocks;
  }
  bool winProposeSortition(level_t proposeLevel, uint64_t threshold);
  friend ProposeModelFace;
  
 private:
  bool getShardedTrxs(uint total_shard, uint my_shard, vec_trx_t& sharded_trx);

  static std::atomic<uint64_t> num_proposed_blocks;
  bool stopped_ = true;
  ProposerConfig conf_;
  uint total_trx_shards_;
  uint my_trx_shard_;
  std::weak_ptr<DagManager> dag_mgr_;
  std::weak_ptr<TransactionManager> trx_mgr_;
  std::weak_ptr<FullNode> full_node_;
  std::shared_ptr<std::thread> proposer_worker_;
  std::unique_ptr<ProposeModelFace> propose_model_;
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
