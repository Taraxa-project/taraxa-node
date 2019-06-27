/*
 * @Copyright: Taraxa.io
 * @Author: Chia-Chun Lin
 * @Date: 2019-03-20 22:01:11
 * @Last Modified by: Chia-Chun Lin
 * @Last Modified time: 2019-03-20 22:07:47
 */

#pragma once
#include <atomic>
#include <condition_variable>
#include <iostream>
#include <memory>
#include <set>
#include <thread>
#include "libdevcore/Log.h"
#include "pbft_chain.hpp"
#include "transaction.hpp"

namespace taraxa {
/**
 * Executor will execute transactions in parallel inside a block,
 * Blocks are sequentially executed.
 * Cannot call execute() until all trans in this period are processed. This will
 * be a blocking call.
 */
class FullNode;
class Executor {
 public:
  using uLock = std::unique_lock<std::mutex>;
  enum class ExecutorStatus { idle, run_parallel, run_sequential };
  Executor() {}
  ~Executor();
  void start();
  void setFullNode(std::shared_ptr<FullNode> node) { node_ = node; }
  void stop();
  void clear();
  bool execute(TrxSchedule const& schedule,
      std::unordered_map<addr_t, bal_t>& sortition_account_balance_table);
  bool executeBlkTrxs(blk_hash_t const& blk,
      std::unordered_map<addr_t, bal_t>& sortition_account_balance_table);
  bool coinTransfer(Transaction const& trx,
      std::unordered_map<addr_t, bal_t>& sortition_account_balance_table);

 private:
  ExecutorStatus status_ = ExecutorStatus::idle;
  bool stopped_ = true;
  std::weak_ptr<FullNode> node_;
  std::shared_ptr<SimpleDBFace> db_blks_;
  std::shared_ptr<SimpleDBFace> db_trxs_;
  std::shared_ptr<SimpleDBFace> db_accs_;
  dev::Logger log_er_{
      dev::createLogger(dev::Verbosity::VerbosityError, "EXETOR")};
  dev::Logger log_wr_{
      dev::createLogger(dev::Verbosity::VerbosityWarning, "EXETOR")};
  dev::Logger log_nf_{
      dev::createLogger(dev::Verbosity::VerbosityInfo, "EXETOR")};
};
}  // namespace taraxa
