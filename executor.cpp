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
  for (auto i = 0; i < num_parallel_executor_; ++i) {
    parallel_executors_.emplace_back(
        std::thread([this]() { executeSingleTrx(); }));
  }
}
void Executor::stop() {
  if (stopped_) return;
}
bool Executor::execute(TrxSchedule const& epoch_trxs) {
  uLock execute_lock(mutex_for_executor_);
  while (status_ != ExecutorStatus::idle && !stopped_) {
    cond_for_executor_.wait(execute_lock);
  }
  if (stopped_) return false;
  assert(status_ == ExecutorStatus::idle);
  all_trx_enqued_ = false;
  status_ = ExecutorStatus::run_parallel;
  // now idle, can put to queue
  // read all trx and put to queue
  std::vector<vec_trx_t> const& schedule = epoch_trxs.schedules;
  for (auto const& blk_trx : schedule) {
    for (auto const& trx : blk_trx) {
      std::string trx_json = db_trxs_->get(trx.toString());
      assert(!trx_json.empty());
      Transaction t(trx_json);
      {
        uLock qu_lock(mutex_for_trx_qu_);
        trx_qu_.emplace_back(t);
      }
    }
    cond_for_trx_qu_.notify_all();
  }
  all_trx_enqued_ = true;
  return true;
}
void Executor::executeSingleTrx() {
  while (!stopped_) {
    Transaction trx;
    {
      uLock qu_lock(mutex_for_trx_qu_);
      while (trx_qu_.empty() && !stopped_) {
        cond_for_trx_qu_.wait(qu_lock);
      }
      if (stopped_) return;
      trx = trx_qu_.front();
      trx_qu_.pop_front();
    }
    // do single transaction
    // assume conflict
    bool conflict = trx.getHash().toString()[0] == '3';
    if (conflict) {
      LOG(logger_) << "trx: " << trx.getHash() << " conflicted!!!" << std::endl;
      conflicted_trx_.insert(trx);
    } else {
      LOG(logger_) << "Execute trx: " << trx.getHash() << std::endl;
    }
  }
}

bool Executor::coinTransfer(Transaction const& trx) {
  name_t sender = trx.getSender();
  name_t receiver = trx.getReceiver();
  bal_t value = trx.getValue();
  bal_t initial_coin = stoull(db_accs_->get(sender.toString()));
  if (initial_coin < trx.getValue()) {
    LOG(logger_) << "Error! Insufficient fund for transfer ..." << std::endl;
    return false;
  }
  return true;
}

}  // namespace taraxa