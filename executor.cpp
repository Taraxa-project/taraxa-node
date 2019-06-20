/*
 * @Copyright: Taraxa.io
 * @Author: Chia-Chun Lin
 * @Date: 2019-03-20 22:11:46
 * @Last Modified by: Chia-Chun Lin
 * @Last Modified time: 2019-03-20 22:11:46
 */
#include "executor.hpp"
#include "full_node.hpp"
#include "util.hpp"

namespace taraxa {
Executor::~Executor() {
  if (!stopped_) {
    stop();
  }
}
void Executor::start() {
  if (!stopped_) return;
  if (!node_.lock()) {
    LOG(log_er_) << "FullNode is not set ..." << std::endl;
    return;
  }
  db_blks_ = node_.lock()->getBlksDB();
  db_trxs_ = node_.lock()->getTrxsDB();
  db_accs_ = node_.lock()->getAccsDB();
  stopped_ = false;
}
void Executor::stop() {
  if (stopped_) return;
  db_blks_ = nullptr;
  db_trxs_ = nullptr;
  db_accs_ = nullptr;
}
bool Executor::executeBlkTrxs(blk_hash_t const& blk) {
  std::string blk_json = db_blks_->get(blk.toString());
  if (blk_json.empty()) {
    LOG(log_er_) << "Cannot get block from db: " << blk << std::endl;
    return false;
  }
  DagBlock dag_block(blk_json);
  auto& log_time = node_.lock()->getTimeLogger();

  auto trxs_hash = dag_block.getTrxs();
  // sequential execute transaction
  for (auto const& trx_hash : trxs_hash) {
    LOG(log_time) << "Transaction " << trx_hash
                  << " read from db at: " << getCurrentTimeMilliSeconds();
    Transaction trx(db_trxs_->get(trx_hash.toString()));
    if (!trx.getHash()) {
      LOG(log_er_) << "Transaction is invalid" << std::endl;
      continue;
    }
    coinTransfer(trx);
    if (node_.lock()) {
      LOG(log_time) << "Transaction " << trx_hash
                    << " executed at: " << getCurrentTimeMilliSeconds();
    }
  }
  db_trxs_->commit();
  return true;
}
bool Executor::execute(TrxSchedule const& sche) {
  for (auto const& blk : sche.blk_order) {
    if (!executeBlkTrxs(blk)) {
      return false;
    }
  }
  return true;
}

bool Executor::coinTransfer(Transaction const& trx) {
  addr_t sender = trx.getSender();
  addr_t receiver = trx.getReceiver();
  bal_t value = trx.getValue();
  auto sender_bal = db_accs_->get(sender.toString());
  auto receiver_bal = db_accs_->get(receiver.toString());
  bal_t sender_initial_coin = sender_bal.empty() ? 0 : stoull(sender_bal);
  bal_t receiver_initial_coin = receiver_bal.empty() ? 0 : stoull(receiver_bal);

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
  db_accs_->update(sender.toString(), std::to_string(new_sender_bal));
  db_accs_->update(receiver.toString(), std::to_string(new_receiver_bal));
  LOG(log_nf_) << "New sender bal: " << new_sender_bal << std::endl;
  LOG(log_nf_) << "New receiver bal: " << new_receiver_bal << std::endl;

  return true;
}

}  // namespace taraxa