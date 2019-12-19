#include "executor.hpp"

#include <stdexcept>

#include "full_node.hpp"
#include "transaction.hpp"
#include "util/eth.hpp"

namespace taraxa {
using dev::KeyPair;
using dev::eth::CheckTransaction;
using dev::eth::Transactions;
using std::move;
using trx_engine::StateTransitionResult;
using trx_engine::TrxEngine;

Executor::Executor(
    uint64_t pbft_require_sortition_coins,
    decltype(log_time_) log_time,  //
    decltype(db_blks_) db_blks,
    decltype(db_trxs_) db_trxs,                                      //
    decltype(replay_protection_service_) replay_protection_service,  //
    decltype(eth_service_) eth_service,                              //
    decltype(db_status_) db_status,                                  //
    bool use_basic_executor)
    : pbft_require_sortition_coins_(pbft_require_sortition_coins),
      log_time_(std::move(log_time)),
      db_blks_(std::move(db_blks)),
      db_trxs_(std::move(db_trxs)),
      replay_protection_service_(std::move(replay_protection_service)),
      db_status_(std::move(db_status)),
      eth_service_(std::move(eth_service)),
      trx_engine_(eth_service_->getAccountsStateDBRaw()),
      use_basic_executor_(use_basic_executor) {
  auto blk_count = db_status_->lookup(
      util::eth::toSlice((uint8_t)taraxa::StatusDbField::ExecutedBlkCount));
  if (!blk_count.empty()) {
    num_executed_blk_.store(*(unsigned long*)&blk_count[0]);
  }
  auto trx_count = db_status_->lookup(
      util::eth::toSlice((uint8_t)taraxa::StatusDbField::ExecutedTrxCount));
  if (!trx_count.empty()) {
    num_executed_trx_.store(*(unsigned long*)&trx_count[0]);
  }
}

bool Executor::execute(PbftBlock const& pbft_block,
                       BalanceTable& sortition_account_balance_table,
                       uint64_t period) {
  auto const& schedule = pbft_block.getScheduleBlock().getSchedule();
  auto dag_blk_count = schedule.dag_blks_order.size();
  EthTransactions transactions(dag_blk_count);
  for (auto blk_i(0); blk_i < dag_blk_count; ++blk_i) {
    auto& blk_hash = schedule.dag_blks_order[blk_i];
    if (db_blks_->lookup(blk_hash).empty()) {
      LOG(log_er_) << "Cannot get block from db: " << blk_hash << std::endl;
      return false;
    }
    auto& dag_blk_trxs_mode = schedule.trxs_mode[blk_i];
    transactions.reserve(transactions.size() + dag_blk_trxs_mode.size() - 1);
    for (auto& trx_hash_and_mode : dag_blk_trxs_mode) {
      auto& [trx_hash, mode] = trx_hash_and_mode;
      // TODO: should not have overlapped transactions anymore
      if (mode == 0) {
        LOG(log_dg_) << "Transaction " << trx_hash << "in block " << blk_hash
                     << " is overlapped";
        continue;
      }
      transactions.emplace_back(db_trxs_->lookup(trx_hash),
                                CheckTransaction::None);
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

    // TODO
    //      auto period_int64 = static_cast<int64_t>(period);
    //      auto const& sender = trx.sender();
    //      auto sender_entry = std_map_entry(result.updatedBalances, sender);
    //      // Update PBFT sortition account last period seen sending trxs
    //      if (sender_entry) {
    //        sender_entry.last_period_seen = static_cast<int64_t>(period);
    //        sender_entry.status = new_change;
    //      }
    //      if (auto e = std_map_entry(result.updatedBalances, sender); e) {
    //        auto const& new_sender_bal = e->second;
    //        LOG(log_dg_) << "Update sender bal: " << sender << " --> "
    //                     << new_sender_bal << " in period " << period;
    //        if (new_sender_bal >= pbft_require_sortition_coins_) {
    //          if (!sender_entry) {
    //            sortition_account_balance_table.emplace(sender,
    //            new_sender_bal,
    //                                                    period_int64,
    //                                                    new_change);
    //          } else {
    //            auto& v = sender_entry->second;
    //            v.balance = new_sender_bal;
    //            v.last_period_seen = period_int64;
    //            v.status = updated;
    //          }
    //        } else if (sender_entry) {
    //          sender_entry.balance = new_sender_bal;
    //          sender_entry.status = remove;
    //        }
    //      }
    //      if (trx.isCreation()) {
    //        continue;
    //      }
    //      auto receiver = trx.to();
    //      if (auto e = std_map_entry(result.updatedBalances, receiver); e) {
    //        auto const& new_receiver_bal = e->second;
    //        LOG(log_dg_) << "New receiver bal: " << receiver << " --> "
    //                     << new_receiver_bal << " in period " << period;
    //        if (new_receiver_bal >= pbft_require_sortition_coins_) {
    //          if (std_map_entry(sortition_account_balance_table, receiver))
    //          {
    //            // update receiver account
    //            sortition_account_balance_table[receiver].balance =
    //                new_receiver_bal;
    //            sortition_account_balance_table[receiver].status =
    //            new_change;
    //          } else {
    //            // New receiver account
    //            sortition_account_balance_table[receiver] =
    //            PbftSortitionAccount(
    //                receiver, new_receiver_bal, -1, new_change);
    //          }
    //        }
    //      }
  }
  // TODO transactional
  eth_service_->commitBlock(pending_header,
                            transactions,  //
                            execution_result.receipts,
                            execution_result.stateRoot);
  // TODO transactional
  replay_protection_service_->commit(period, transactions);
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