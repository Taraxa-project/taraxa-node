/*
 * @Copyright: Taraxa.io
 * @Author: Chia-Chun Lin
 * @Date: 2019-03-20 22:11:46
 * @Last Modified by: Chia-Chun Lin
 * @Last Modified time: 2019-03-20 22:11:46
 */
#include "executor.hpp"

namespace taraxa {

bool Executor::execute(
    TrxSchedule const& sche,
    std::unordered_map<addr_t, val_t>& sortition_account_balance_table) {
  for (auto const& blk : sche.blk_order) {
    if (!executeBlkTrxs(blk, sortition_account_balance_table)) {
      return false;
    }
  }
  auto const& root = db_accs_->commitToTrie();
  for (auto& blk : sche.blk_order) {
    assert(state_root_registry_->putStateRoot(blk, root));
  }
  state_root_registry_->setCurrentBlock(sche.blk_order.back());
  return true;
}

bool Executor::executeBlkTrxs(
    blk_hash_t const& blk,
    std::unordered_map<addr_t, val_t>& sortition_account_balance_table) {
  std::string blk_json = db_blks_->get(blk.toString());
  if (blk_json.empty()) {
    LOG(log_er_) << "Cannot get block from db: " << blk << std::endl;
    return false;
  }
  DagBlock dag_block(blk_json);

  auto trxs_hash = dag_block.getTrxs();
  // sequential execute transaction
  for (auto const& trx_hash : trxs_hash) {
    LOG(log_time_) << "Transaction " << trx_hash
                   << " read from db at: " << getCurrentTimeMilliSeconds();
    Transaction trx(db_trxs_->get(trx_hash.toString()));
    if (!trx.getHash()) {
      LOG(log_er_) << "Transaction is invalid: " << trx << std::endl;
      continue;
    }
    coinTransfer(trx, sortition_account_balance_table);
    LOG(log_time_) << "Transaction " << trx_hash
                   << " executed at: " << getCurrentTimeMilliSeconds();
  }
  return true;
}

bool Executor::coinTransfer(
    Transaction const& trx,
    std::unordered_map<addr_t, val_t>& sortition_account_balance_table) {
  addr_t sender = trx.getSender();
  addr_t receiver = trx.getReceiver();
  val_t value = trx.getValue();
  auto sender_bal = db_accs_->get(sender.toString());
  auto receiver_bal = db_accs_->get(receiver.toString());
  val_t sender_initial_coin = sender_bal.empty() ? 0 : stoull(sender_bal);
  val_t receiver_initial_coin = receiver_bal.empty() ? 0 : stoull(receiver_bal);

  if (sender_initial_coin < trx.getValue()) {
    LOG(log_er_) << "Insufficient fund for transfer ... , sender " << sender
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
  db_accs_->update(sender.toString(), toString(new_sender_bal));
  db_accs_->update(receiver.toString(), toString(new_receiver_bal));
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

  LOG(log_nf_) << "New sender bal: " << new_sender_bal << std::endl;
  LOG(log_nf_) << "New receiver bal: " << new_receiver_bal << std::endl;

  return true;
}

}  // namespace taraxa