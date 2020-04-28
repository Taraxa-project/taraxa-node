#include "executor.hpp"

namespace taraxa {
using trx_engine::StateTransitionResult;
using trx_engine::TrxEngine;

Executor::Executor(
    decltype(log_time_) log_time, decltype(eth_service_) eth_service)
    : log_time_(std::move(log_time)),
      eth_service_(std::move(eth_service)),
      trx_engine_(eth_service_->getStateDB()) {}

std::optional<dev::eth::BlockHeader> Executor::execute(
    DbStorage::BatchPtr const& batch, PbftBlock const& pbft_block,
    EthTransactions &transactions,
    unordered_map<addr_t, val_t>& execution_touched_account_balances) {
  auto [pending_header, current_header, trx_scope] = eth_service_->startBlock(
      batch, pbft_block.getBeneficiary(), pbft_block.getTimestamp());
  StateTransitionResult execution_result;
  if (transactions.empty()) {
    execution_result.stateRoot = current_header.stateRoot();
  } else {
    try {
      // TODO op blockhash
      execution_result = trx_engine_.transitionStateAndCommit({
          current_header.stateRoot(),
          trx_engine::Block{
              pending_header.number(),
              pending_header.author(),
              pending_header.timestamp(),
              pending_header.difficulty(),
              uint64_t(pending_header.gasLimit()),
              transactions,
          },
      });
    } catch (TrxEngine::Exception const& e) {
      // TODO more precise error handling
      // TODO propagate the exception
      LOG(log_er_) << e.what() << std::endl;
      return std::nullopt;
    }
  }
  auto& new_eth_header = eth_service_->commitBlock(pending_header,
                                                   transactions,
                                                   execution_result.receipts,
                                                   execution_result.stateRoot);

  // Copy the period execution touched account balances
  execution_touched_account_balances =
      execution_result.touchedExternallyOwnedAccountBalances;

  // Check transactions execution error
  for (size_t i(0); i < transactions.size(); ++i) {
    auto const& trx = transactions[i];
    auto trx_hash = trx.sha3();
    LOG(log_time_) << "Transaction " << trx_hash
                   << " executed at: " << getCurrentTimeMilliSeconds();
    auto const& trx_output = execution_result.transactionOutputs[i];
    if (!trx_output.error.empty()) {
      LOG(log_er_) << "Trx " << trx_hash << " failed: " << trx_output.error
                   << std::endl;
    }
  }
  return move(new_eth_header);
}
}  // namespace taraxa