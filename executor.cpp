#include "executor.hpp"

#include <stdexcept>
#include <unordered_set>

#include "full_node.hpp"
#include "transaction.hpp"

namespace taraxa {
using dev::KeyPair;
using dev::eth::CheckTransaction;
using dev::eth::Transactions;
using std::move;
using std::unordered_set;
using trx_engine::StateTransitionResult;
using trx_engine::TrxEngine;

Executor::Executor(
    decltype(log_time_) log_time, decltype(db_) db,
    decltype(replay_protection_service_) replay_protection_service,
    decltype(eth_service_) eth_service)
    : log_time_(std::move(log_time)),
      db_(std::move(db)),
      replay_protection_service_(std::move(replay_protection_service)),
      eth_service_(std::move(eth_service)),
      trx_engine_(eth_service_->getStateDB()) {
  auto blk_count = db_->getStatusField(taraxa::StatusDbField::ExecutedBlkCount);
  num_executed_blk_.store(blk_count);
  auto trx_count = db_->getStatusField(taraxa::StatusDbField::ExecutedTrxCount);
  num_executed_trx_.store(trx_count);
}

std::optional<dev::eth::BlockHeader> Executor::execute(
    DbStorage::BatchPtr const& batch, PbftBlock const& pbft_block,
    unordered_set<addr_t>& dag_block_proposers,
    unordered_set<addr_t>& trx_senders,
    unordered_map<addr_t, val_t>& execution_touched_account_balances) {
  auto const& schedule = pbft_block.getSchedule();
  auto dag_blk_count = schedule.dag_blks_order.size();
  EthTransactions transactions;
  transactions.reserve(dag_blk_count);
  for (auto blk_i(0); blk_i < dag_blk_count; ++blk_i) {
    auto& blk_hash = schedule.dag_blks_order[blk_i];
    dev::bytes dag_block = db_->getDagBlockRaw(blk_hash);
    if (dag_block.empty()) {
      LOG(log_er_) << "Cannot get block from db: " << blk_hash << std::endl;
      return std::nullopt;
    }
    dag_block_proposers.insert(DagBlock(dag_block).getSender());
    auto& dag_blk_trxs_mode = schedule.trxs_mode[blk_i];
    transactions.reserve(transactions.capacity() + dag_blk_trxs_mode.size() -
                         1);
    for (auto& trx_hash_and_mode : dag_blk_trxs_mode) {
      auto& [trx_hash, mode] = trx_hash_and_mode;
      auto& trx = transactions.emplace_back(db_->getTransactionRaw(trx_hash),
                                            CheckTransaction::None);
      trx_senders.insert(trx.sender());
      db_->removePendingTransactionToBatch(batch, trx_hash);
      LOG(log_time_) << "Transaction " << trx_hash
                     << " read from db at: " << getCurrentTimeMilliSeconds();
    }
  }
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
  auto& new_eth_header = eth_service_->commitBlock(pending_header,  //
                                                   transactions,    //
                                                   execution_result.receipts,
                                                   execution_result.stateRoot);
  uint64_t period = pbft_block.getPeriod();
  replay_protection_service_->commit(batch, period, transactions);

  // Copy the period execution touched account balances
  execution_touched_account_balances =
      execution_result.touchedExternallyOwnedAccountBalances;

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
  if (dag_blk_count != 0) {
    num_executed_blk_.fetch_add(dag_blk_count);
    num_executed_trx_.fetch_add(transactions.size());
    LOG(log_nf_) << getFullNodeAddress() << " : Executed dag blocks #"
                 << num_executed_blk_ - dag_blk_count << "-"
                 << num_executed_blk_ - 1
                 << " , Efficiency: " << transactions.size() << "/"
                 << transactions.capacity();
  }
  return move(new_eth_header);
}

addr_t Executor::getFullNodeAddress() const {
  auto full_node = full_node_.lock();
  if (full_node) {
    return full_node->getAddress();
  } else {
    return addr_t();
  }
}

}  // namespace taraxa