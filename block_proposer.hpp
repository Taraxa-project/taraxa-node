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
#include <vector>
#include "boost/thread.hpp"

namespace taraxa {

class DagManager;
class TransactionManager;

/**
 * Block proproser request for unpacked transaction,
 * will block if block transaction is not ready.
 * Once ready, get unpacked transaction, then query pivot and tips and propose
 */

class BlockProposer {
 public:
  BlockProposer(unsigned num_threads, std::shared_ptr<DagManager> dag_mgr,
                std::shared_ptr<TransactionManager> trx_mgr)
      : num_threads_(num_threads), dag_mgr_(dag_mgr), trx_mgr_(trx_mgr) {}
  ~BlockProposer() {
    if (!stopped_) stop();
  }
  void proposeBlock();
  void start();
  void stop();

  // debug
  void setVerbose(bool verbose);
  static uint64_t getNumProposedBlocks() {
    return BlockProposer::num_proposed_blocks;
  }

 private:
  static std::atomic<uint64_t> num_proposed_blocks;
  bool stopped_ = true;
  unsigned num_threads_;
  std::weak_ptr<DagManager> dag_mgr_;
  std::weak_ptr<TransactionManager> trx_mgr_;
  std::vector<boost::thread> proposer_threads_;

  dev::Logger log_er_{
      dev::createLogger(dev::Verbosity::VerbosityError, "blk_pp")};
  dev::Logger log_wr_{
      dev::createLogger(dev::Verbosity::VerbosityWarning, "blk_pp")};
  dev::Logger log_nf_{
      dev::createLogger(dev::Verbosity::VerbosityInfo, "blk_pp")};
};

}  // namespace taraxa

#endif
