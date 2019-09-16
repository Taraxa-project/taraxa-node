/*
 * @Copyright: Taraxa.io
 * @Author: Chia-Chun Lin
 * @Date: 2019-03-20 22:01:11
 * @Last Modified by: Chia-Chun Lin
 * @Last Modified time: 2019-03-20 22:07:47
 */
#ifndef TARAXA_NODE_EXECUTOR_HPP
#define TARAXA_NODE_EXECUTOR_HPP

#pragma once
#include <atomic>
#include <condition_variable>
#include <iostream>
#include <memory>
#include <set>
#include <thread>
#include "dag_block.hpp"
#include "libdevcore/Log.h"
#include "pbft_chain.hpp"
#include "state_registry.hpp"
#include "util.hpp"

namespace taraxa {
/**
 * Executor will execute transactions in parallel inside a block,
 * Blocks are sequentially executed.
 * Cannot call execute() until all trans in this period are processed. This will
 * be a blocking call.
 */
class FullNode;

class Executor {
 private:
  uint64_t pbft_require_sortition_coins_;
  dev::Logger log_time_;
  std::weak_ptr<FullNode> full_node_;
  std::shared_ptr<SimpleDBFace> db_blks_ = nullptr;
  std::shared_ptr<SimpleDBFace> db_trxs_ = nullptr;
  std::shared_ptr<StateRegistry> state_registry_ = nullptr;
  std::atomic<uint64_t> num_executed_trx_ = 0;
  std::atomic<uint64_t> num_executed_blk_ = 0;

  // for debug purpose
  dev::Logger log_si_{
      dev::createLogger(dev::Verbosity::VerbositySilent, "EXETOR")};
  dev::Logger log_er_{
      dev::createLogger(dev::Verbosity::VerbosityError, "EXETOR")};
  dev::Logger log_wr_{
      dev::createLogger(dev::Verbosity::VerbosityWarning, "EXETOR")};
  dev::Logger log_nf_{
      dev::createLogger(dev::Verbosity::VerbosityInfo, "EXETOR")};
  dev::Logger log_dg_{
      dev::createLogger(dev::Verbosity::VerbosityDebug, "EXETOR")};

 public:
  Executor(uint64_t pbft_require_sortition_coins,
           decltype(log_time_) log_time,  //
           decltype(db_blks_) db_blks,
           decltype(db_trxs_) db_trxs,  //
           decltype(state_registry_) state_registry)
      : pbft_require_sortition_coins_(pbft_require_sortition_coins),
        log_time_(std::move(log_time)),
        db_blks_(std::move(db_blks)),
        db_trxs_(std::move(db_trxs)),
        state_registry_(std::move(state_registry)) {}

  bool execute(TrxSchedule const& schedule,
               std::unordered_map<addr_t, std::pair<val_t, int64_t>>&
                   sortition_account_balance_table,
               uint64_t period);
  uint64_t getNumExecutedTrx() { return num_executed_trx_; }
  uint64_t getNumExecutedBlk() { return num_executed_blk_; }
  void setFullNode(std::shared_ptr<FullNode> full_node) {
    full_node_ = full_node;
  }

 private:
  bool executeBlkTrxs(
      StateRegistry::State&, blk_hash_t const& blk,
      std::vector<uint> const& trx_modes,
      std::unordered_map<addr_t, std::pair<val_t, int64_t>>& sortition_account_balance_table,
      uint64_t period);
  bool coinTransfer(
      StateRegistry::State&, Transaction const& trx,
      std::unordered_map<addr_t, std::pair<val_t, int64_t>>& sortition_account_balance_table,
      uint64_t period, DagBlock const& dag_block);
};

}  // namespace taraxa

#endif