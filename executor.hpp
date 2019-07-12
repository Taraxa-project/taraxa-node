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
class DagBlock;

class Executor {
 public:
  inline static const auto MOCK_BLOCK_GAS_LIMIT =
      std::numeric_limits<uint64_t>::max();

  using uLock = std::unique_lock<std::mutex>;
  enum class ExecutorStatus { idle, run_parallel, run_sequential };
  Executor() {}
  ~Executor();
  // fixme: start/stop methods seem to be excessive, and complicate the
  // lifecycle
  void start();
  void setFullNode(std::shared_ptr<FullNode> node) { node_ = node; }
  void stop();
  void clear();
  bool execute(
      TrxSchedule const& schedule,
      std::unordered_map<addr_t, bal_t>& sortition_account_balance_table);
  bool executeBlkTrxs(
      blk_hash_t const& blk,
      std::unordered_map<addr_t, bal_t>& sortition_account_balance_table);
  bool coinTransfer(
      Transaction const& trx,
      std::unordered_map<addr_t, bal_t>& sortition_account_balance_table);

 private:
  ExecutorStatus status_ = ExecutorStatus::idle;
  bool stopped_ = true;
  std::weak_ptr<FullNode> node_;
  std::shared_ptr<SimpleDBFace> db_blks_ = nullptr;
  std::shared_ptr<SimpleDBFace> db_trxs_ = nullptr;
  std::shared_ptr<SimpleDBFace> db_accs_ = nullptr;
  dev::Logger log_er_{
      dev::createLogger(dev::Verbosity::VerbosityError, "EXETOR")};
  dev::Logger log_wr_{
      dev::createLogger(dev::Verbosity::VerbosityWarning, "EXETOR")};
  dev::Logger log_nf_{
      dev::createLogger(dev::Verbosity::VerbosityInfo, "EXETOR")};
};

using TransactionOverlapTable =
    std::vector<std::pair<blk_hash_t, std::vector<bool>>>;
enum class TransactionExecStatus { non_ordered, ordered, executed };
using TransactionExecStatusTable =
    StatusTable<trx_hash_t, TransactionExecStatus>;
using TrxOverlapInBlock = std::pair<blk_hash_t, std::vector<bool>>;

// TODO: the table need to flush out
// Compute 1) transaction order and 2) map[transaction --> dagblock]
class TransactionOrderManager {
 public:
  TransactionOrderManager() = default;
  ~TransactionOrderManager();
  void start();
  void setFullNode(std::shared_ptr<FullNode> node) { node_ = node; }
  void stop();
  void clear() { status_.clear(); }

  std::vector<bool> computeOrderInBlock(DagBlock const& blk);
  std::shared_ptr<std::vector<TrxOverlapInBlock>> computeOrderInBlocks(
      std::vector<std::shared_ptr<DagBlock>> const& blks);
  blk_hash_t getDagBlockFromTransaction(trx_hash_t const& t);

 private:
  bool stopped_ = true;
  std::weak_ptr<FullNode> node_;
  TransactionExecStatusTable status_;
  std::shared_ptr<SimpleDBFace> db_trxs_to_blk_ = nullptr;
  dev::Logger log_er_{
      dev::createLogger(dev::Verbosity::VerbosityError, "TRXODR")};
  dev::Logger log_wr_{
      dev::createLogger(dev::Verbosity::VerbosityWarning, "TRXODR")};
  dev::Logger log_nf_{
      dev::createLogger(dev::Verbosity::VerbosityInfo, "TRXODR")};
};

}  // namespace taraxa
