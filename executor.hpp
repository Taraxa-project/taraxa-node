#ifndef TARAXA_NODE_EXECUTOR_HPP
#define TARAXA_NODE_EXECUTOR_HPP

#pragma once
#include <atomic>
#include <condition_variable>
#include <iostream>
#include <memory>
#include <set>
#include <thread>
#include "account_state/index.hpp"
#include "dag_block.hpp"
#include "database_face_cache.hpp"
#include "libdevcore/Log.h"
#include "pbft_chain.hpp"
#include "pbft_sortition_account.h"
#include "replay_protection/index.hpp"
#include "trx_engine/index.hpp"
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
  std::shared_ptr<DatabaseFaceCache> db_blks_ = nullptr;
  std::shared_ptr<DatabaseFaceCache> db_trxs_ = nullptr;
  std::shared_ptr<account_state::StateRegistry> state_registry_ = nullptr;
  using ReplayProtectionService = replay_protection::ReplayProtectionService;
  std::shared_ptr<dev::db::DatabaseFace> db_status_ = nullptr;
  std::shared_ptr<ReplayProtectionService> replay_protection_service_;
  trx_engine::TrxEngine trx_engine_;
  bool use_basic_executor_;
  std::atomic<uint64_t> num_executed_trx_ = 0;
  std::atomic<uint64_t> num_executed_blk_ = 0;
  using BalanceTable = std::unordered_map<addr_t, PbftSortitionAccount>;
  using AccountNonceTable = StatusTable<addr_t, val_t>;
  AccountNonceTable accs_nonce_;

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
           decltype(db_trxs_) db_trxs,                                      //
           decltype(replay_protection_service_) replay_protection_service,  //
           decltype(state_registry_) state_registry,                        //
           decltype(db_status_) db_status,                                  //
           bool use_basic_executor = false);

  bool execute(TrxSchedule const& schedule,
               BalanceTable& sortition_account_balance_table, uint64_t period);

  uint64_t getNumExecutedTrx() { return num_executed_trx_; }
  uint64_t getNumExecutedBlk() { return num_executed_blk_; }
  void setFullNode(std::shared_ptr<FullNode> full_node) {
    full_node_ = full_node;
  }

 private:
  bool execute_main(TrxSchedule const& schedule,
                    BalanceTable& sortition_account_balance_table,
                    uint64_t period,
                    ReplayProtectionService::transaction_batch_t& executed_trx);
  bool execute_basic(
      TrxSchedule const& schedule,
      BalanceTable& sortition_account_balance_table, uint64_t period,
      ReplayProtectionService::transaction_batch_t& executed_trx);
  bool executeBlkTrxs(
      account_state::State&, blk_hash_t const& blk,
      std::vector<uint> const& trx_modes,
      BalanceTable& sortition_account_balance_table, uint64_t period,
      ReplayProtectionService::transaction_batch_t& executed_trx);
  bool coinTransfer(account_state::State&, Transaction const& trx,
                    BalanceTable& sortition_account_balance_table,
                    uint64_t period,  //
                    DagBlock const& dag_block);
  addr_t getFullNodeAddress() const;
};

}  // namespace taraxa

#endif