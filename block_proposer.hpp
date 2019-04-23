/*
 * @Copyright: Taraxa.io
 * @Author: Chia-Chun Lin
 * @Date: 2019-01-24 11:30:32
 * @Last Modified by: Chia-Chun Lin
 * @Last Modified time: 2019-01-25 17:15:20
 */

#ifndef BLOCK_PROPOSER_HPP
#define BLOCK_PROPOSER_HPP
#include <libdevcore/Log.h>
#include <random>
#include <thread>
#include <vector>
#include "boost/thread.hpp"

namespace taraxa {

class DagManager;
class TransactionManager;
class FullNode;
class BlockProposer;

struct ProposerConfig {
  uint mode;
  uint param1;
  uint param2;
};

class ProposeModelFace {
 public:
  virtual ~ProposeModelFace() {}
  virtual bool propose() = 0;
  void setProposer(std::shared_ptr<BlockProposer> proposer) {
    proposer_ = proposer;
  }

 protected:
  std::weak_ptr<BlockProposer> proposer_;
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
      : conf_(conf), dag_mgr_(dag_mgr), trx_mgr_(trx_mgr) {
    if (conf_.mode == 0) {
      propose_model_ =
          std::make_unique<RandomPropose>(conf_.param1, conf_.param2);
    }
  }
  ~BlockProposer() {
    if (!stopped_) stop();
  }
  void setFullNode(std::shared_ptr<FullNode> full_node) {
    full_node_ = full_node;
  }
  void proposeBlock();
  void start();
  void stop();
  std::shared_ptr<BlockProposer> getShared();

  // debug
  static uint64_t getNumProposedBlocks() {
    return BlockProposer::num_proposed_blocks;
  }
  friend ProposeModelFace;

 private:
  static std::atomic<uint64_t> num_proposed_blocks;
  bool stopped_ = true;
  ProposerConfig conf_;
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
  dev::Logger log_tr_{
      dev::createLogger(dev::Verbosity::VerbosityTrace, "BLK_PP")};
};

}  // namespace taraxa

#endif
