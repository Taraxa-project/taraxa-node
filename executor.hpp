#ifndef TARAXA_NODE_EXECUTOR_HPP
#define TARAXA_NODE_EXECUTOR_HPP

#pragma once
#include <libdevcore/Log.h>

#include <atomic>
#include <condition_variable>
#include <iostream>
#include <memory>
#include <set>
#include <thread>

#include "dag_block.hpp"
#include "db_storage.hpp"
#include "eth/eth_service.hpp"
#include "pbft_chain.hpp"
#include "pbft_sortition_account.hpp"
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
  std::shared_ptr<DbStorage> db_ = nullptr;
  std::shared_ptr<eth::eth_service::EthService> eth_service_;
  using ReplayProtectionService = replay_protection::ReplayProtectionService;
  std::shared_ptr<ReplayProtectionService> replay_protection_service_;
  trx_engine::TrxEngine trx_engine_;
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
           decltype(db_) db,
           decltype(replay_protection_service_) replay_protection_service,  //
           decltype(eth_service_) eth_service);

  // TODO return richer context (actual transactions, receipts, etc.)
  std::optional<dev::eth::BlockHeader> execute(
      PbftBlock const& pbft_block,
      BalanceTable& sortition_account_balance_table, uint64_t period);

  uint64_t getNumExecutedTrx() { return num_executed_trx_; }
  uint64_t getNumExecutedBlk() { return num_executed_blk_; }
  void setFullNode(std::shared_ptr<FullNode> full_node) {
    full_node_ = full_node;
  }

 private:
  addr_t getFullNodeAddress() const;
};

}  // namespace taraxa

#endif