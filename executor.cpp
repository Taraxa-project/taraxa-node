/*
 * @Copyright: Taraxa.io
 * @Author: Chia-Chun Lin
 * @Date: 2019-03-20 22:11:46
 * @Last Modified by: Chia-Chun Lin
 * @Last Modified time: 2019-03-20 22:11:46
 */

#include "executor.hpp"
#include "full_node.hpp"
#include "transaction.hpp"

namespace taraxa {

bool Executor::execute(
    TrxSchedule const& sche,
    std::unordered_map<addr_t, val_t>& sortition_account_balance_table) {
  if (sche.blk_order.empty()) {
    return true;
  }
  // TODO this state instance is reusable
  auto state = state_registry_->getCurrentState();
  for (auto i(0); i < sche.blk_order.size(); ++i) {
    auto blk = sche.blk_order[i];
    auto trx_modes = sche.vec_trx_modes[i];
    if (!executeBlkTrxs(state, blk, trx_modes,
                        sortition_account_balance_table)) {
      return false;
    }
  }
  state_registry_->commitAndPush(state, sche.blk_order);
  return true;
}

bool Executor::executeBlkTrxs(
    StateRegistry::State& state, blk_hash_t const& blk,
    std::vector<uint> const& trx_modes,
    std::unordered_map<addr_t, val_t>& sortition_account_balance_table) {
  std::string blk_json = db_blks_->get(blk.toString());
  if (blk_json.empty()) {
    LOG(log_er_) << "Cannot get block from db: " << blk << std::endl;
    return false;
  }
  DagBlock dag_block(blk_json);

  auto trxs_hash = dag_block.getTrxs();
  // sequential execute transaction

  for (auto i(0); i < trxs_hash.size(); ++i) {
    auto const& trx_hash = trxs_hash[i];
    auto mode = trx_modes[i];
    if (mode == 0) {
      LOG(log_dg_) << "Transaction " << trx_hash << "in block " << blk
                   << " is overlapped";
      continue;
    }
    LOG(log_time_) << "Transaction " << trx_hash
                   << " read from db at: " << getCurrentTimeMilliSeconds();
    Transaction trx(db_trxs_->get(trx_hash.toString()));
    if (!trx.getHash()) {
      LOG(log_er_) << "Transaction is invalid: " << trx << std::endl;
      continue;
    }
    coinTransfer(state, trx, sortition_account_balance_table);
    LOG(log_time_) << "Transaction " << trx_hash
                   << " executed at: " << getCurrentTimeMilliSeconds();
  }
  num_executed_blk_.fetch_add(1);
  LOG(log_nf_) << full_node_.lock()->getAddress() << ": Block number "
               << num_executed_blk_ << ": " << blk << " executed";
  return true;
}

bool Executor::coinTransfer(
    StateRegistry::State& state, Transaction const& trx,
    std::unordered_map<addr_t, val_t>& sortition_account_balance_table) {
  addr_t sender = trx.getSender();
  addr_t receiver = trx.getReceiver();
  val_t value = trx.getValue();
  val_t sender_initial_coin = state.balance(sender);
  val_t receiver_initial_coin = state.balance(receiver);
  if (sender_initial_coin < trx.getValue()) {
    LOG(log_dg_) << "Insufficient fund for transfer ... , sender " << sender
                 << " , sender balance: " << sender_initial_coin
                 << " , transfer: " << value << std::endl;
    return false;
  }
  if (receiver_initial_coin >
      std::numeric_limits<uint64_t>::max() - trx.getValue()) {
    LOG(log_er_) << "Error! Fund can overflow ..." << std::endl;
    return false;
  }
  val_t new_sender_bal = sender_initial_coin - value;
  val_t new_receiver_bal = receiver_initial_coin + value;
  state.setBalance(sender, new_sender_bal);
  state.setBalance(receiver, new_receiver_bal);
  // Update account balance table. Will remove in VM since vm return a list of
  // modified balance accounts
  if (new_sender_bal >= pbft_require_sortition_coins_) {
    sortition_account_balance_table[sender] = new_sender_bal;
  } else if (sortition_account_balance_table.find(sender) !=
             sortition_account_balance_table.end()) {
    sortition_account_balance_table.erase(sender);
  }
  if (new_receiver_bal >= pbft_require_sortition_coins_) {
    sortition_account_balance_table[receiver] = new_receiver_bal;
  }

  LOG(log_dg_) << "Update sender bal: " << sender << " --> " << new_sender_bal
               << std::endl;
  LOG(log_dg_) << "New receiver bal: " << receiver << " --> "
               << new_receiver_bal << std::endl;
  num_executed_trx_.fetch_add(1);
  return true;
}

}  // namespace taraxa