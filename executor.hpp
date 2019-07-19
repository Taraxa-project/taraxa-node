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
#include "SimpleStateDBDelegate.h"
#include "StateRegistry.hpp"
#include "dag_block.hpp"
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

class Executor {
  uint64_t pbft_require_sortition_coins_;
  dev::Logger log_time_;
  std::shared_ptr<SimpleDBFace> db_blks_ = nullptr;
  std::shared_ptr<SimpleDBFace> db_trxs_ = nullptr;
  std::shared_ptr<SimpleStateDBDelegate> db_accs_ = nullptr;
  std::shared_ptr<StateRegistry> state_root_registry_ = nullptr;
  dev::Logger log_er_{
      dev::createLogger(dev::Verbosity::VerbosityError, "EXETOR")};
  dev::Logger log_wr_{
      dev::createLogger(dev::Verbosity::VerbosityWarning, "EXETOR")};
  dev::Logger log_nf_{
      dev::createLogger(dev::Verbosity::VerbosityInfo, "EXETOR")};

 public:
  inline static const auto MOCK_BLOCK_GAS_LIMIT =
      std::numeric_limits<uint64_t>::max();

  Executor(uint64_t pbft_require_sortition_coins,
           decltype(log_time_) log_time,  //
           decltype(db_blks_) db_blks,
           decltype(db_trxs_) db_trxs,  //
           decltype(db_accs_) db_accs,
           decltype(state_root_registry_) state_root_registry)
      : pbft_require_sortition_coins_(pbft_require_sortition_coins),
        log_time_(std::move(log_time)),
        db_blks_(std::move(db_blks)),
        db_trxs_(std::move(db_trxs)),
        db_accs_(std::move(db_accs)),
        state_root_registry_(std::move(state_root_registry)) {}

  bool execute(
      TrxSchedule const& schedule,
      std::unordered_map<addr_t, val_t>& sortition_account_balance_table);

 private:
  bool executeBlkTrxs(
      blk_hash_t const& blk,
      std::unordered_map<addr_t, val_t>& sortition_account_balance_table);
  bool coinTransfer(
      Transaction const& trx,
      std::unordered_map<addr_t, val_t>& sortition_account_balance_table);
};

}  // namespace taraxa

#endif