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
#include "rocks_db.hpp"
#include "transaction.hpp"

namespace taraxa {
/**
 * Executor will execute transactions in parallel inside a block,
 * Blocks are sequentially executed.
 * Cannot call execute() until all trans in thie epoch are processed. This will
 * be a blocking call.
 */

class Executor {
 public:
  using uLock = std::unique_lock<std::mutex>;
  enum class ExecutorStatus { idle, run_parallel, run_sequential };
  Executor(size_t num_parallel_executor, std::shared_ptr<RocksDb> db_blks,
           std::shared_ptr<RocksDb> db_trxs, std::shared_ptr<RocksDb> db_accs)
      : num_parallel_executor_(num_parallel_executor),
        db_blks_(db_blks),
        db_trxs_(db_trxs),
        db_accs_(db_accs) {}
  ~Executor();
  void start();
  void stop();
  void clear();
  bool execute(TrxSchedule const& schedule);
  bool executeBlkTrxs(vec_trx_t const& trxs);
  void executeSingleTrx();
  bool coinTransfer(Transaction const& trx);

 private:
  ExecutorStatus status_ = ExecutorStatus::idle;
  bool stopped_ = true;
  bool all_trx_enqued_ = false;
  size_t num_parallel_executor_;
  std::vector<std::thread> parallel_executors_;

  std::mutex mutex_for_executor_;
  std::condition_variable cond_for_executor_;

  std::deque<Transaction> trx_qu_;
  std::mutex mutex_for_trx_qu_;
  std::condition_variable cond_for_trx_qu_;

  std::shared_ptr<RocksDb> db_blks_;
  std::shared_ptr<RocksDb> db_trxs_;
  std::shared_ptr<RocksDb> db_accs_;
  std::set<Transaction> conflicted_trx_;

  std::atomic<unsigned> num_parallel_executed_trx = 0;
  std::atomic<unsigned> num_conflicted_trx = 0;
  dev::Logger logger_{dev::createLogger(dev::Verbosity::VerbosityInfo, "exe")};
  dev::Logger logger_dbg_{
      dev::createLogger(dev::Verbosity::VerbosityDebug, "exe")};
};
}  // namespace taraxa
