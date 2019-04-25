/*
 * @Copyright: Taraxa.io
 * @Author: Chia-Chun Lin
 * @Date: 2019-03-20 22:11:46
 * @Last Modified by: Chia-Chun Lin
 * @Last Modified time: 2019-03-20 22:11:46
 */
#include "executor.hpp"
namespace taraxa {
Executor::~Executor() {
  if (!stopped_) {
    stop();
  }
}
void Executor::start() {
  if (!stopped_) return;
  stopped_ = false;
}
void Executor::stop() {
  if (stopped_) return;
}

bool Executor::executeBlkTrxs(dev::eth::State& state, const dev::OverlayDB& db_, blk_hash_t const& blk) {
  string block_string = db_.lookup(blk);
  bool empty = block_string.empty();
  DagBlock dag_block;
  if(!empty) {
    dag_block = DagBlock(block_string);
  }
  if (empty || !dag_block.isValid()) {
    LOG(log_er_) << "Cannot get block from db: " << blk << std::endl;
    return false;
  }
  auto trxs_hash = dag_block.getTrxs();
  // sequential execute transaction
  for (auto const& trx_hash : trxs_hash) {
    Transaction trx(db_.lookup(trx_hash));
    if (!trx.getHash()) {
      LOG(log_er_) << "Transaction is invalid" << std::endl;
      continue;
    }
    coinTransfer(state, trx);
  }
  state.commit(dev::eth::State::CommitBehaviour::KeepEmptyAccounts);
  return true;
}
    
bool Executor::execute(dev::eth::State& state, const dev::OverlayDB& db_, TrxSchedule const& sche) {
  for (auto const& blk : sche.blk_order) {
    executeBlkTrxs(state, db_, blk);
  }
  return true;
}

bool Executor::coinTransfer(dev::eth::State& state, Transaction const& trx) {
  addr_t sender = trx.getSender();
  addr_t receiver = trx.getReceiver();
  bal_t value = trx.getValue();
  auto sender_bal = state.balance(sender);
  auto receiver_bal = state.balance(receiver);
  bal_t sender_initial_coin = sender_bal;
  bal_t receiver_initial_coin = receiver_bal;

  if (sender_initial_coin < trx.getValue()) {
    LOG(log_er_) << "Error! Insufficient fund for transfer ..." << std::endl;
    return false;
  }
  if (receiver_initial_coin >
      std::numeric_limits<uint64_t>::max() - trx.getValue()) {
    LOG(log_er_) << "Error! Fund can overflow ..." << std::endl;
    return false;
  }
  bal_t new_sender_bal = sender_initial_coin - value;
  bal_t new_receiver_bal = receiver_initial_coin + value;
  state.setBalance(sender, new_sender_bal);
  state.setBalance(receiver, new_receiver_bal);
  LOG(log_nf_) << "New sender bal: " << new_sender_bal << std::endl;
  LOG(log_nf_) << "New receiver bal: " << new_receiver_bal << std::endl;

  return true;
}

}  // namespace taraxa
