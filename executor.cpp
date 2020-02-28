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
      trx_engine_(eth_service_->getStateDB()) {
  auto blk_count = db_->getStatusField(taraxa::StatusDbField::ExecutedBlkCount);
  num_executed_blk_.store(blk_count);
  auto trx_count = db_->getStatusField(taraxa::StatusDbField::ExecutedTrxCount);
  num_executed_trx_.store(trx_count);
}

std::optional<dev::eth::BlockHeader> Executor::execute(
    DbStorage::BatchPtr const& batch, PbftBlock const& pbft_block,
    BalanceTable& sortition_account_balance_table) {
  auto const& schedule = pbft_block.getSchedule();
  auto dag_blk_count = schedule.dag_blks_order.size();
  EthTransactions transactions;
  transactions.reserve(dag_blk_count);
  unordered_set<addr_t> dag_block_proposers;
  unordered_set<addr_t> trx_senders;
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
  // Update PBFT sortition table for DAG block proposers who don't have account
  // balance changed (no transaction relative accounts)
  for (auto& addr : dag_block_proposers) {
    auto sortition_table_entry =
        std_find(sortition_account_balance_table, addr);
    if (sortition_table_entry) {
      auto& pbft_sortition_account = (*sortition_table_entry)->second;
      // fixme: weird lossy cast
      pbft_sortition_account.last_period_seen = static_cast<int64_t>(period);
      pbft_sortition_account.status = new_change;
    }
  }
  // Update PBFT sortition table for DAG block proposers who have account
  // balance changed
  for (auto& [addr, balance] :
       execution_result.touchedExternallyOwnedAccountBalances) {
    auto is_proposer = bool(std_find(dag_block_proposers, addr));
    auto is_sender = bool(std_find(trx_senders, addr));
    LOG(log_dg_) << "Externally owned account (is_sender: " << is_sender
                 << ") balance update: " << addr << " --> " << balance
                 << " in period " << period;
    auto enough_balance = balance >= pbft_require_sortition_coins_;
    auto sortition_table_entry =
        std_find(sortition_account_balance_table, addr);
    if (is_sender) {
      // Transaction sender
      if (sortition_table_entry) {
        auto& pbft_sortition_account = (*sortition_table_entry)->second;
        pbft_sortition_account.balance = balance;
        if (is_proposer) {
          // fixme: weird lossy cast
          pbft_sortition_account.last_period_seen =
              static_cast<int64_t>(period);
        }
        if (enough_balance) {
          pbft_sortition_account.status = new_change;
        } else {
          // After send coins doesn't have enough for sortition
          pbft_sortition_account.status = remove;
        }
      }
    } else {
      // Receiver
      if (enough_balance) {
        if (sortition_table_entry) {
          auto& pbft_sortition_account = (*sortition_table_entry)->second;
          pbft_sortition_account.balance = balance;
          if (is_proposer) {
            // TODO: weird lossy cast
            pbft_sortition_account.last_period_seen =
                static_cast<int64_t>(period);
          }
          pbft_sortition_account.status = new_change;
        } else {
          int64_t last_seen_period = -1;
          if (is_proposer) {
            // TODO: weird lossy cast
            last_seen_period = static_cast<int64_t>(period);
          }
          sortition_account_balance_table[addr] =
              PbftSortitionAccount(addr, balance, last_seen_period, new_change);
        }
      }
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