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
#include "libdevcore/OverlayDB.h"
#include "libethereum/State.h"

namespace taraxa {
/**
 * Executor will execute transactions in parallel inside a block,
 * Blocks are sequentially executed.
 * Cannot call execute() until all trans in this epoch are processed. This will
 * be a blocking call.
 */

class Executor {
 public:
  using uLock = std::unique_lock<std::mutex>;
  enum class ExecutorStatus { idle, run_parallel, run_sequential };
    Executor(dev::eth::State const & state)
      : state_(state) {}
  ~Executor();
  void start();
  void stop();
  void clear();
  bool execute(TrxSchedule const& schedule);
  bool executeBlkTrxs(blk_hash_t const& blk);
  bool coinTransfer(Transaction const& trx);

 private:
  ExecutorStatus status_ = ExecutorStatus::idle;
  bool stopped_ = true;

  dev::eth::State state_;

  dev::Logger log_er_{
      dev::createLogger(dev::Verbosity::VerbosityError, "EXETOR")};
  dev::Logger log_wr_{
      dev::createLogger(dev::Verbosity::VerbosityWarning, "EXETOR")};
  dev::Logger log_nf_{
      dev::createLogger(dev::Verbosity::VerbosityInfo, "EXETOR")};
};
}  // namespace taraxa
