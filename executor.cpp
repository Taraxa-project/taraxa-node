#include "executor.hpp"
#include <stdexcept>
#include "full_node.hpp"
#include "transaction.hpp"
#include "util/eth.hpp"

namespace taraxa {
using trx_engine::StateTransitionResult;
using trx_engine::TrxEngine;

Executor::Executor(
    uint64_t pbft_require_sortition_coins,
    decltype(log_time_) log_time,  //
    decltype(db_blks_) db_blks,
    decltype(db_trxs_) db_trxs,                                      //
    decltype(replay_protection_service_) replay_protection_service,  //
    decltype(state_registry_) state_registry,                        //
    decltype(db_status_) db_status,                                  //
    bool use_basic_executor)
    : pbft_require_sortition_coins_(pbft_require_sortition_coins),
      log_time_(std::move(log_time)),
      db_blks_(std::move(db_blks)),
      db_trxs_(std::move(db_trxs)),
      replay_protection_service_(std::move(replay_protection_service)),
      db_status_(std::move(db_status)),
      state_registry_(std::move(state_registry)),
      trx_engine_({state_registry_->getAccountDbRaw(), noop()}),
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

bool Executor::execute(TrxSchedule const& schedule,
                       BalanceTable& sortition_account_balance_table,
                       uint64_t period) {
  ReplayProtectionService::transaction_batch_t executed_trx;
  bool success = false;
  if (this->use_basic_executor_) {
    success = execute_basic(schedule, sortition_account_balance_table, period,
                            executed_trx);
  } else {
    success = execute_main(schedule, sortition_account_balance_table, period,
                           executed_trx);
  }
  if (success) {
    replay_protection_service_->commit(period, executed_trx);
  }
  return success;
}

bool Executor::execute_main(
    TrxSchedule const& schedule, BalanceTable& sortition_account_balance_table,
    uint64_t period,
    ReplayProtectionService::transaction_batch_t& executed_trx) {
  if (schedule.dag_blks_order.empty()) {
    return true;
  }
  for (auto blk_i(0); blk_i < schedule.dag_blks_order.size(); ++blk_i) {
    auto blk_hash = schedule.dag_blks_order[blk_i];
    auto blk_bytes = db_blks_->lookup(blk_hash);
    if (blk_bytes.size() == 0) {
      LOG(log_er_) << "Cannot get block from db: " << blk_hash << std::endl;
      return false;
    }
    DagBlock dag_block(blk_bytes);
    auto dag_blk_trxs_mode = schedule.trxs_mode[blk_i];
    auto num_trxs = dag_blk_trxs_mode.size();
    int num_overlapped_trx(0);
    // TODO pass by reference
    std::vector<trx_engine::Transaction> trx_to_execute;
    for (auto trx_i(0); trx_i < num_trxs; ++trx_i) {
      auto const& trx_hash = dag_blk_trxs_mode[trx_i].first;
      LOG(log_time_) << "Transaction " << trx_hash
                     << " read from db at: " << getCurrentTimeMilliSeconds();
      auto trx = std::make_shared<Transaction>(db_trxs_->lookup(trx_hash));
      if (!trx->getHash()) {
        LOG(log_er_) << "Transaction is invalid: " << trx << std::endl;
        continue;
      }
      // Update PBFT sortition account last period seen sending trxs
      auto const& trx_sender = trx->getSender();
      if (sortition_account_balance_table.find(trx_sender) !=
          sortition_account_balance_table.end()) {
        sortition_account_balance_table[trx_sender].last_period_seen =
            static_cast<int64_t>(period);
        sortition_account_balance_table[trx_sender].status = new_change;
      }
      auto mode = dag_blk_trxs_mode[trx_i].second;
      // TODO: should not have overlapped transactions anymore
      if (mode == 0) {
        LOG(log_dg_) << "Transaction " << trx_hash << "in block " << blk_hash
                     << " is overlapped";
        num_overlapped_trx++;
        continue;
      }
      auto const& receiver = trx->getReceiver();
      trx_to_execute.push_back({
          receiver.isZero() ? std::nullopt : std::optional(receiver),
          trx->getSender(),
          // TODO unint64 is correct
          uint64_t(trx->getNonce()),
          trx->getValue(),
          // TODO unint64 is correct
          uint64_t(trx->getGas()),
          trx->getGasPrice(),
          trx->getData(),
          trx_hash,
      });
      executed_trx.push_back(trx);
    }
    if (!trx_to_execute.empty()) {
      auto const& snapshot = state_registry_->getCurrentSnapshot();
      StateTransitionResult result;
      try {
        result = trx_engine_.transitionState({
            snapshot.state_root,
            trx_engine::Block{
                snapshot.block_number + 1,
                dag_block.getSender(),
                dag_block.getTimestamp(),
                0,
                // TODO unint64 is correct
                uint64_t(MOCK_BLOCK_GAS_LIMIT),
                blk_hash,
                trx_to_execute,
            },
        });
        trx_engine_.commitToDisk(result.stateRoot);
      } catch (TrxEngine::Exception const& e) {
        // TODO better error handling
        LOG(log_er_) << e.what() << std::endl;
        return false;
      }
      state_registry_->append({{blk_hash, result.stateRoot}});
      auto current_state = state_registry_->getCurrentState();
      for (auto i(0); i < trx_to_execute.size(); ++i) {
        auto const& trx = trx_to_execute[i];
        auto const& receipt = result.receipts[i];
        if (!receipt.error.empty()) {
          LOG(log_er_) << "Trx " << trx.hash << " failed: " << receipt.error
                       << std::endl;
        }
        auto const& sender = trx.from;
        auto const& receiver = *(trx.to);
        auto const& new_sender_bal = current_state.balance(sender);
        auto const& new_receiver_bal = current_state.balance(receiver);
        if (new_sender_bal >= pbft_require_sortition_coins_) {
          sortition_account_balance_table[sender].balance = new_sender_bal;
        } else if (sortition_account_balance_table.find(sender) !=
                   sortition_account_balance_table.end()) {
          sortition_account_balance_table[sender].balance = new_sender_bal;
          sortition_account_balance_table[sender].status = remove;
        }
        if (new_receiver_bal >= pbft_require_sortition_coins_) {
          if (sortition_account_balance_table.find(receiver) !=
              sortition_account_balance_table.end()) {
            // update receiver account
            sortition_account_balance_table[receiver].balance =
                new_receiver_bal;
            sortition_account_balance_table[receiver].status = new_change;
          } else {
            // New receiver account
            sortition_account_balance_table[receiver] = PbftSortitionAccount(
                receiver, new_receiver_bal, -1, new_change);
          }
        }
        LOG(log_dg_) << "Update sender bal: " << sender << " --> "
                     << new_sender_bal << " in period " << period;
        LOG(log_dg_) << "New receiver bal: " << receiver << " --> "
                     << new_receiver_bal << " in period " << period;
        num_executed_trx_.fetch_add(1);
        LOG(log_time_) << "Transaction " << trx.hash
                       << " executed at: " << getCurrentTimeMilliSeconds();
      }
    }
    num_executed_blk_.fetch_add(1);
    LOG(log_si_) << getFullNodeAddress() << " : Block number "
                 << num_executed_blk_ << ": " << blk_hash
                 << " executed, Efficiency: " << (num_trxs - num_overlapped_trx)
                 << " / " << num_trxs;
  }
  return true;
}

bool Executor::execute_basic(
    TrxSchedule const& sche, BalanceTable& sortition_account_balance_table,
    uint64_t period,
    ReplayProtectionService::transaction_batch_t& executed_trx) {
  if (sche.dag_blks_order.empty()) {
    return true;
  }
  // TODO this state instance is reusable
  auto state = state_registry_->getCurrentState();

  for (auto i(0); i < sche.dag_blks_order.size(); ++i) {
    auto blk = sche.dag_blks_order[i];
    auto dag_blk_trxs_mode = sche.trxs_mode[i];
    if (!executeBlkTrxs(state, blk, dag_blk_trxs_mode,
                        sortition_account_balance_table, period,
                        executed_trx)) {
      return false;
    }
  }
  state_registry_->commitAndPush(state, sche.dag_blks_order);
  return true;
}

bool Executor::executeBlkTrxs(
    account_state::State& state, blk_hash_t const& blk,
    std::vector<std::pair<trx_hash_t, uint>> const& dag_blk_trxs_mode,
    BalanceTable& sortition_account_balance_table, uint64_t period,
    ReplayProtectionService::transaction_batch_t& executed_trx) {
  std::string blk_json = db_blks_->lookup(blk.toString());
  auto blk_bytes = db_blks_->lookup(blk);
  if (blk_bytes.size() == 0) {
    LOG(log_er_) << "Cannot get block from db: " << blk << std::endl;
    return false;
  }

  DagBlock dag_block(blk_bytes);

  auto num_trxs = dag_blk_trxs_mode.size();
  // sequential execute transaction
  int num_overlapped_trx = 0;

  for (auto i(0); i < num_trxs; ++i) {
    auto const& trx_hash = dag_blk_trxs_mode[i].first;
    LOG(log_time_) << "Transaction " << trx_hash
                   << " read from db at: " << getCurrentTimeMilliSeconds();
    auto trx = std::make_shared<Transaction>(db_trxs_->lookup(trx_hash));
    if (!trx->getHash()) {
      LOG(log_er_) << "Transaction is invalid: " << trx << std::endl;
      continue;
    }
    // Update PBFT sortition account last period seen sending trxs
    auto const& trx_sender = trx->getSender();
    if (sortition_account_balance_table.find(trx_sender) !=
        sortition_account_balance_table.end()) {
      sortition_account_balance_table[trx_sender].last_period_seen =
          static_cast<int64_t>(period);
      sortition_account_balance_table[trx_sender].status = new_change;
    }
    // TODO: should not have overlapped transactions
    auto mode = dag_blk_trxs_mode[i].second;
    if (mode == 0) {
      LOG(log_dg_) << "Transaction " << trx_hash << "in block " << blk
                   << " is overlapped";
      num_overlapped_trx++;
      continue;
    }
    if (coinTransfer(state, *trx, sortition_account_balance_table, period,
                     dag_block)) {
      executed_trx.push_back(trx);
      num_executed_trx_.fetch_add(1);
    }
    LOG(log_time_) << "Transaction " << trx_hash
                   << " executed at: " << getCurrentTimeMilliSeconds();
  }
  num_executed_blk_.fetch_add(1);
  LOG(log_si_) << getFullNodeAddress() << " : Block number "
               << num_executed_blk_ << ": " << blk
               << " executed, Efficiency: " << (num_trxs - num_overlapped_trx)
               << " / " << num_trxs;
  return true;
}

bool Executor::coinTransfer(account_state::State& state, Transaction const& trx,
                            BalanceTable& sortition_account_balance_table,
                            uint64_t period, DagBlock const& dag_block) {
  auto hash = trx.getHash();
  val_t value = trx.getValue();
  addr_t sender = trx.getSender();
  addr_t receiver = trx.getReceiver();
  val_t sender_initial_coin = state.balance(sender);
  val_t receiver_initial_coin = state.balance(receiver);
  val_t new_sender_bal = sender_initial_coin;
  val_t new_receiver_bal = receiver_initial_coin;
  state.setNonce(sender, state.getNonce(sender) + 1);
  if (sender != receiver) {
    if (sender_initial_coin < trx.getValue()) {
      LOG(log_nf_) << "Insufficient fund for transfer ... , sender " << sender
                   << " , sender balance: " << sender_initial_coin
                   << " , transfer: " << value;
      return false;
    }
    if (receiver_initial_coin > std::numeric_limits<val_t>::max() - value) {
      LOG(log_er_) << "Fund can overflow ...";
      return false;
    }
    new_sender_bal = sender_initial_coin - value;
    new_receiver_bal = receiver_initial_coin + value;
    state.setBalance(receiver, new_receiver_bal);

    val_t nonce = trx.getNonce();
    auto [prev_nonce, exist] = accs_nonce_.get(sender);
    accs_nonce_.update(sender, nonce);
  }
  // if sender == receiver and trx.value == 0, we should still call setBalance
  // to create an empty account
  state.setBalance(sender, new_sender_bal);
  // Update PBFT account balance table. Will remove in VM since vm return a list
  // of modified balance accounts
  if (new_sender_bal >= pbft_require_sortition_coins_) {
    sortition_account_balance_table[sender].balance = new_sender_bal;
  } else if (sortition_account_balance_table.find(sender) !=
             sortition_account_balance_table.end()) {
    sortition_account_balance_table[sender].balance = new_sender_bal;
    sortition_account_balance_table[sender].status = remove;
  }
  if (new_receiver_bal >= pbft_require_sortition_coins_) {
    if (sortition_account_balance_table.find(receiver) !=
        sortition_account_balance_table.end()) {
      // update receiver account
      sortition_account_balance_table[receiver].balance = new_receiver_bal;
      sortition_account_balance_table[receiver].status = new_change;
    } else {
      // New receiver account
      sortition_account_balance_table[receiver] =
          PbftSortitionAccount(receiver, new_receiver_bal, -1, new_change);
    }
  }

  LOG(log_dg_) << "Update sender bal: " << sender << " --> " << new_sender_bal
               << " in period " << period;
  LOG(log_dg_) << "New receiver bal: " << receiver << " --> "
               << new_receiver_bal << " in period " << period;
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