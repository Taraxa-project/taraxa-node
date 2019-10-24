#include "executor.hpp"
#include <stdexcept>
#include "full_node.hpp"
#include "transaction.hpp"

namespace taraxa {
using trx_engine::StateTransitionResult;
using trx_engine::TrxEngine;

bool Executor::execute(TrxSchedule const& schedule,
                       BalanceTable& sortition_account_balance_table,
                       uint64_t period) {
  if (this->use_basic_executor_) {
    return execute_basic(schedule, sortition_account_balance_table, period);
  }
  return execute_main(schedule, sortition_account_balance_table, period);
}

bool Executor::execute_main(TrxSchedule const& schedule,
                            BalanceTable& sortition_account_balance_table,
                            uint64_t period) {
  if (schedule.blk_order.empty()) {
    return true;
  }
  for (auto blk_i(0); blk_i < schedule.blk_order.size(); ++blk_i) {
    auto blk_hash = schedule.blk_order[blk_i];
    auto blk_bytes = db_blks_->get(blk_hash);
    if (blk_bytes.size() == 0) {
      LOG(log_er_) << "Cannot get block from db: " << blk_hash << std::endl;
      return false;
    }
    DagBlock dag_block(blk_bytes);
    auto trx_modes = schedule.vec_trx_modes[blk_i];
    auto trxs_hashes = dag_block.getTrxs();
    auto num_trxs = trxs_hashes.size();
    int num_overlapped_trx(0);
    // TODO pass by reference
    std::vector<trx_engine::Transaction> trx_to_execute;
    for (auto trx_i(0); trx_i < num_trxs; ++trx_i) {
      auto const& trx_hash = trxs_hashes[trx_i];
      auto mode = trx_modes[trx_i];
      if (mode == 0) {
        LOG(log_dg_) << "Transaction " << trx_hash << "in block " << blk_hash
                     << " is overlapped";
        num_overlapped_trx++;
        continue;
      }
      LOG(log_time_) << "Transaction " << trx_hash
                     << " read from db at: " << getCurrentTimeMilliSeconds();
      Transaction trx(db_trxs_->get(trx_hash));
      if (!trx.getHash()) {
        LOG(log_er_) << "Transaction is invalid: " << trx << std::endl;
        continue;
      }
      auto const& receiver = trx.getReceiver();
      trx_to_execute.push_back({
          receiver.isZero() ? std::nullopt : std::optional(receiver),
          trx.getSender(),
          // TODO unint64 is correct
          uint64_t(trx.getNonce()),
          trx.getValue(),
          // TODO unint64 is correct
          uint64_t(trx.getGas()),
          trx.getGasPrice(),
          trx.getData(),
          trx_hash,
      });
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
          sortition_account_balance_table[sender] = PbftSortitionAccount(
              sender, new_sender_bal, static_cast<int64_t>(period), new_change);
        } else if (sortition_account_balance_table.find(sender) !=
                   sortition_account_balance_table.end()) {
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

bool Executor::execute_basic(TrxSchedule const& sche,
                             BalanceTable& sortition_account_balance_table,
                             uint64_t period) {
  if (sche.blk_order.empty()) {
    return true;
  }
  // TODO this state instance is reusable
  auto state = state_registry_->getCurrentState();

  for (auto i(0); i < sche.blk_order.size(); ++i) {
    auto blk = sche.blk_order[i];
    auto trx_modes = sche.vec_trx_modes[i];
    if (!executeBlkTrxs(state, blk, trx_modes, sortition_account_balance_table,
                        period)) {
      return false;
    }
  }
  state_registry_->commitAndPush(state, sche.blk_order);
  return true;
}

bool Executor::executeBlkTrxs(StateRegistry::State& state,
                              blk_hash_t const& blk,
                              std::vector<uint> const& trx_modes,
                              BalanceTable& sortition_account_balance_table,
                              uint64_t period) {
  std::string blk_json = db_blks_->get(blk.toString());
  auto blk_bytes = db_blks_->get(blk);
  if (blk_bytes.size() == 0) {
    LOG(log_er_) << "Cannot get block from db: " << blk << std::endl;
    return false;
  }

  DagBlock dag_block(blk_bytes);

  auto trxs_hash = dag_block.getTrxs();
  auto num_trxs = trxs_hash.size();
  // sequential execute transaction
  int num_overlapped_trx = 0;

  for (auto i(0); i < trxs_hash.size(); ++i) {
    auto const& trx_hash = trxs_hash[i];
    auto mode = trx_modes[i];
    if (mode == 0) {
      LOG(log_dg_) << "Transaction " << trx_hash << "in block " << blk
                   << " is overlapped";
      num_overlapped_trx++;
      continue;
    }
    LOG(log_time_) << "Transaction " << trx_hash
                   << " read from db at: " << getCurrentTimeMilliSeconds();
    Transaction trx(db_trxs_->get(trx_hash));
    if (!trx.getHash()) {
      LOG(log_er_) << "Transaction is invalid: " << trx << std::endl;
      continue;
    }
    coinTransfer(state, trx, sortition_account_balance_table, period,
                 dag_block);
    num_executed_trx_.fetch_add(1);
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

bool Executor::coinTransfer(StateRegistry::State& state, Transaction const& trx,
                            BalanceTable& sortition_account_balance_table,
                            uint64_t period, DagBlock const& dag_block) {
  auto hash = trx.getHash();
  val_t value = trx.getValue();
  addr_t sender = trx.getSender();
  {
    auto nonce = state.getNonce(sender);
    if (nonce != trx.getNonce()) {
      LOG(log_nf_) << fmt(
          "Invalid nonce. account: %s, trx: %s, expected nonce: "
          "%s, actual "
          "nonce: %s",
          sender, hash, trx.getNonce(), nonce);
      return false;
    }
    state.setNonce(sender, nonce + 1);
  }
  addr_t receiver = trx.getReceiver();
  val_t sender_initial_coin = state.balance(sender);
  val_t receiver_initial_coin = state.balance(receiver);
  val_t new_sender_bal = sender_initial_coin;
  val_t new_receiver_bal = receiver_initial_coin;
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
    if (exist) {
      if (prev_nonce + 1 != nonce) {
        LOG(log_er_) << "Trx: " << hash << " Sender " << sender
                     << " nonce has gap, prev nonce " << prev_nonce
                     << " curr nonce " << nonce << " Receiver: " << receiver
                     << " value " << value;
        // return false;
      }
    } else {
      if (nonce != 0) {
        LOG(log_er_) << "Trx: " << hash << " Sender " << sender
                     << " nonce has gap, prev nonce unavailable1! (expectt 0)"
                     << " curr nonce " << nonce << " Receiver: " << receiver
                     << " value " << value;
        // return false;
      }
    }
    accs_nonce_.update(sender, nonce);
  }
  // if sender == receiver and trx.value == 0, we should still call setBalance
  // to create an empty account
  state.setBalance(sender, new_sender_bal);
  // Update PBFT account balance table. Will remove in VM since vm return a list
  // of modified balance accounts
  if (new_sender_bal >= pbft_require_sortition_coins_) {
    sortition_account_balance_table[sender] = PbftSortitionAccount(
        sender, new_sender_bal, static_cast<int64_t>(period), new_change);
  } else if (sortition_account_balance_table.find(sender) !=
             sortition_account_balance_table.end()) {
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