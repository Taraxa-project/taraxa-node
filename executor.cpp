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
  bool stopped_ = false;
}
void Executor::stop() {
  if (stopped_) return;
}
bool Executor::executeBlkTrxs(blk_hash_t const& blk) {
  DagBlock dag_block(db_blks_->get(blk.toString()));
  if (!dag_block.isValid()) {
    LOG(log_er_) << "Cannot get block from db: " << blk << std::endl;
    return false;
  }
  auto trxs_hash = dag_block.getTrxs();
  // sequential execute transaction
  for (auto const& trx_hash : trxs_hash) {
    Transaction trx(db_trxs_->get(trx_hash.toString()));
    if (!trx.getHash()) {
      LOG(log_er_) << "Transaction is invalid" << std::endl;
      continue;
    }
    coinTransfer(trx);
  }
}
bool Executor::execute(TrxSchedule const& sche) {
  for (auto const& blk : sche.blk_order) {
    executeBlkTrxs(blk);
  }
  return true;
}

bool Executor::coinTransfer(Transaction const& trx) {
  addr_t sender = trx.getSender();
  addr_t receiver = trx.getReceiver();
  bal_t value = trx.getValue();
  bal_t sender_initial_coin = stoull(db_accs_->get(sender.toString()));
  bal_t receiver_initial_coin = stoull(db_accs_->get(receiver.toString()));

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
  db_accs_->put(sender.toString(), std::to_string(new_sender_bal));
  db_accs_->put(receiver.toString(), std::to_string(new_receiver_bal));
  return true;
}

}  // namespace taraxa