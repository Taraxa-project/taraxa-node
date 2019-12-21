#include "executor.hpp"

#include <stdexcept>
#include <unordered_set>

#include "full_node.hpp"
#include "transaction.hpp"
#include "util/eth.hpp"

namespace taraxa {
using dev::KeyPair;
using dev::eth::CheckTransaction;
using dev::eth::Transactions;
using std::move;
using std::unordered_set;
using trx_engine::StateTransitionResult;
using trx_engine::TrxEngine;

Executor::Executor(
    uint64_t pbft_require_sortition_coins,
    decltype(log_time_) log_time,  //
    decltype(db_) db,
    decltype(replay_protection_service_) replay_protection_service,  //
    decltype(eth_service_) eth_service)
    : pbft_require_sortition_coins_(pbft_require_sortition_coins),
      log_time_(std::move(log_time)),
      db_(std::move(db)),
      replay_protection_service_(std::move(replay_protection_service)),
      eth_service_(std::move(eth_service)),
      trx_engine_(eth_service_->getAccountsStateDBRaw()) {
  auto blk_count = db_->getStatusField(taraxa::StatusDbField::ExecutedBlkCount);
  num_executed_blk_.store(blk_count);
  auto trx_count = db_->getStatusField(taraxa::StatusDbField::ExecutedTrxCount);
  num_executed_trx_.store(trx_count);
}

bool Executor::execute(PbftBlock const& pbft_block,
                       BalanceTable& sortition_account_balance_table,
                       uint64_t period) {
  auto const& schedule = pbft_block.getScheduleBlock().getSchedule();
  auto dag_blk_count = schedule.dag_blks_order.size();
  EthTransactions transactions;
  transactions.reserve(dag_blk_count);
  unordered_set<addr_t> senders;
  for (auto blk_i(0); blk_i < dag_blk_count; ++blk_i) {
    auto& blk_hash = schedule.dag_blks_order[blk_i];
    if (db_->getDagBlockRaw(blk_hash).empty()) {
      LOG(log_er_) << "Cannot get block from db: " << blk_hash << std::endl;
      return false;
    }
    auto& dag_blk_trxs_mode = schedule.trxs_mode[blk_i];
    transactions.reserve(transactions.capacity() + dag_blk_trxs_mode.size() -
                         1);
    for (auto& trx_hash_and_mode : dag_blk_trxs_mode) {
      auto& [trx_hash, mode] = trx_hash_and_mode;
      // TODO: should not have overlapped transactions anymore
      if (mode == 0) {
        LOG(log_dg_) << "Transaction " << trx_hash << "in block " << blk_hash
                     << " is overlapped";
        continue;
      }
      auto& trx = transactions.emplace_back(
          db_->getTransactionRaw(trx_hash), CheckTransaction::None);
      senders.insert(trx.sender());
      LOG(log_time_) << "Transaction " << trx_hash
                     << " read from db at: " << getCurrentTimeMilliSeconds();
    }
  }
  auto [pending_header, current_header] = eth_service_->startBlock(
      pbft_block.getAuthor(), pbft_block.getTimestamp());
  StateTransitionResult execution_result;
  if (transactions.empty()) {
    execution_result.stateRoot = current_header.stateRoot();
  } else {
    try {
      // TODO transactional
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
      LOG(log_er_) << e.what() << std::endl;
      return false;
    }
  }
  // TODO transactional
  eth_service_->commitBlock(pending_header,
                            transactions,  //
                            execution_result.receipts,
                            execution_result.stateRoot);
  // TODO transactional
  replay_protection_service_->commit(period, transactions);
  for (auto& [addr, balance] :
       execution_result.touchedExternallyOwnedAccountBalances) {
    auto enough_balance = balance >= pbft_require_sortition_coins_;
    auto sortition_table_entry =
        std_find(sortition_account_balance_table, addr);
    auto is_sender = bool(std_find(senders, addr));
    LOG(log_dg_) << "Externally owned account (is_sender: " << is_sender
                 << ") balance update: " << addr << " --> " << balance
                 << " in period " << period;
    if (is_sender) {
      if (sortition_table_entry) {
        auto& v = (*sortition_table_entry)->second;
        // fixme: weird lossy cast
        v.last_period_seen = static_cast<int64_t>(period);
        v.status = new_change;
      }
      if (enough_balance) {
        sortition_account_balance_table[addr].balance = balance;
      } else if (sortition_table_entry) {
        auto& v = (*sortition_table_entry)->second;
        v.balance = balance;
        v.status = remove;
      }
      continue;
    }
    if (!enough_balance) {
      continue;
    }
    if (sortition_table_entry) {
      auto& v = (*sortition_table_entry)->second;
      v.balance = balance;
      v.status = new_change;
    } else {
      sortition_account_balance_table[addr] =
          PbftSortitionAccount(addr, balance, -1, new_change);
    }
  }
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
    LOG(log_si_) << getFullNodeAddress() << " : Executed dag blocks #"
                 << num_executed_blk_ - dag_blk_count << "-"
                 << num_executed_blk_ - 1
                 << " , Efficiency: " << transactions.size() << "/"
                 << transactions.capacity();
  }
  return true;
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