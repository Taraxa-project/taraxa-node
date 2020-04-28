#ifndef TARAXA_NODE_EXECUTOR_HPP
#define TARAXA_NODE_EXECUTOR_HPP

#pragma once
#include <libdevcore/Log.h>

#include "eth/eth_service.hpp"
#include "trx_engine/index.hpp"

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
  dev::Logger log_time_;
  std::shared_ptr<eth::eth_service::EthService> eth_service_;
  trx_engine::TrxEngine trx_engine_;

  dev::Logger log_er_{
      dev::createLogger(dev::Verbosity::VerbosityError, "EXETOR")};

 public:
  Executor(decltype(log_time_) log_time, decltype(eth_service_) eth_service);

  std::optional<dev::eth::BlockHeader> execute(
      DbStorage::BatchPtr const& batch, PbftBlock const& pbft_block,
      EthTransactions& transactions,
      unordered_map<addr_t, val_t>& execution_touched_account_balances);
};

}  // namespace taraxa

#endif