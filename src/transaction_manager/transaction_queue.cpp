#include "transaction_queue.hpp"

#include <string>
#include <utility>

#include "transaction.hpp"

namespace taraxa {

void TransactionQueue::start() {
  if (bool b = true; !stopped_.compare_exchange_strong(b, !b)) {
    return;
  }
}

void TransactionQueue::stop() {
  if (bool b = false; !stopped_.compare_exchange_strong(b, !b)) {
    return;
  }
  cond_for_unverified_qu_.notify_all();
}

void TransactionQueue::insert(Transaction const &trx, bool verify) {
  trx_hash_t hash = trx.getHash();
  listIter iter;
  {
    uLock lock(main_shared_mutex_);
    iter = trx_buffer_.insert(trx_buffer_.end(), trx);
    assert(iter != trx_buffer_.end());
    queued_trxs_[trx.getHash()] = iter;
    if (verify) {
      verified_trxs_[trx.getHash()] = iter;
      new_verified_transactions_ = true;
    } else {
      uLock unlock(shared_mutex_for_unverified_qu_);
      unverified_hash_qu_.emplace_back(std::make_pair(hash, iter));
      cond_for_unverified_qu_.notify_one();
    }
  }
  LOG(log_nf_) << " Trx: " << hash << " inserted. " << verify << std::endl;
}

void TransactionQueue::insertUnverifiedTrxs(const vector<Transaction> &trxs) {
  if (trxs.empty()) {
    return;
  }
  std::vector<listIter> iters;
  iters.reserve(trxs.size());
  {
    listIter iter;
    uLock lock(main_shared_mutex_);
    uLock unverified_lock(shared_mutex_for_unverified_qu_);
    for (const auto &trx : trxs) {
      iter = trx_buffer_.insert(trx_buffer_.end(), trx);
      assert(iter != trx_buffer_.end());
      queued_trxs_[trx.getHash()] = iter;
      iters.push_back(iter);
    }
    for (size_t idx = 0; idx < trxs.size(); idx++) {
      unverified_hash_qu_.emplace_back(std::make_pair(trxs[idx].getHash(), iters[idx]));
      cond_for_unverified_qu_.notify_one();
    }
  }
}

std::pair<trx_hash_t, TransactionQueue::listIter> TransactionQueue::getUnverifiedTransaction() {
  std::pair<trx_hash_t, listIter> item;
  {
    uLock lock(shared_mutex_for_unverified_qu_);
    while (unverified_hash_qu_.empty() && !stopped_) {
      cond_for_unverified_qu_.wait(lock);
    }
    if (stopped_) {
      LOG(log_nf_) << "Transaction verifier stopped ... " << std::endl;
    } else {
      item = unverified_hash_qu_.front();
      unverified_hash_qu_.pop_front();
    }
  }
  return item;
}

void TransactionQueue::removeTransactionFromBuffer(trx_hash_t const &hash) {
  uLock lock(main_shared_mutex_);
  trx_buffer_.erase(queued_trxs_[hash]);
  queued_trxs_.erase(hash);
}

void TransactionQueue::addTransactionToVerifiedQueue(trx_hash_t const &hash, std::list<Transaction>::iterator iter) {
  uLock verify_lock(main_shared_mutex_);
  // This function is called from several background threads by verifyQueuedTrxs()
  // and at the same time verifyBlockTransactions() can be called, that is removing
  // all trxs that are already in DAG block from those queues. So we need to have lock here
  // and check if the transaction was not removed by DAG block
  if (queued_trxs_.find(hash) == queued_trxs_.end()) return;
  verified_trxs_[hash] = iter;
  new_verified_transactions_ = true;
}

// The caller is responsible for storing the transaction to db!
void TransactionQueue::removeBlockTransactionsFromQueue(vec_trx_t const &all_block_trxs) {
  uLock lock(main_shared_mutex_);
  upgradableLock unlock(shared_mutex_for_unverified_qu_);
  if (queued_trxs_.empty()) return;

  std::unordered_map<trx_hash_t, bool> unverif_to_remove;
  std::vector<trx_hash_t> to_remove;
  for (auto const &trx : all_block_trxs) {
    if (auto que_trx = queued_trxs_.find(trx); que_trx != queued_trxs_.end()) {
      to_remove.emplace_back(trx);
      if (auto vrf_trx = verified_trxs_.find(trx); vrf_trx != verified_trxs_.end()) {
        verified_trxs_.erase(trx);
      } else {
        unverif_to_remove[trx] = true;
      }
    }
  }
  if (unverif_to_remove.size()) {
    upgradeLock locked(unlock);
    unverified_hash_qu_.erase(std::remove_if(unverified_hash_qu_.begin(), unverified_hash_qu_.end(),
                                             [&unverif_to_remove](const auto &el) {
                                               return unverif_to_remove.find(el.first) != unverif_to_remove.end();
                                             }),
                              unverified_hash_qu_.end());
  }
  // clear trx_buffer
  for (auto const &t : to_remove) {
    trx_buffer_.erase(queued_trxs_[t]);
    queued_trxs_.erase(t);
  }
}

std::unordered_map<trx_hash_t, Transaction> TransactionQueue::getVerifiedTrxSnapShot() const {
  std::unordered_map<trx_hash_t, Transaction> verified_trxs;
  sharedLock lock(main_shared_mutex_);
  for (auto const &trx : verified_trxs_) {
    verified_trxs[trx.first] = *(trx.second);
  }
  LOG(log_dg_) << "Get: " << verified_trxs.size() << " verified trx out. " << std::endl;
  return verified_trxs;
}

std::vector<Transaction> TransactionQueue::getNewVerifiedTrxSnapShot() {
  std::vector<Transaction> verified_trxs;
  {
    sharedLock lock(main_shared_mutex_);
    if (new_verified_transactions_) {
      new_verified_transactions_ = false;
      std::transform(verified_trxs_.begin(), verified_trxs_.end(), std::back_inserter(verified_trxs),
                     [](std::pair<const taraxa::trx_hash_t, taraxa::TransactionQueue::listIter> const &trx) {
                       return *(trx.second);
                     });
      LOG(log_dg_) << "Get: " << verified_trxs.size() << "verified trx out for gossiping " << std::endl;
    }
  }
  return verified_trxs;
}

// search from queued_trx_
std::shared_ptr<Transaction> TransactionQueue::getTransaction(trx_hash_t const &hash) const {
  sharedLock lock(main_shared_mutex_);
  auto it = queued_trxs_.find(hash);
  if (it != queued_trxs_.end()) {
    return std::make_shared<Transaction>(*(it->second));
  }
  return nullptr;
}

std::unordered_map<trx_hash_t, Transaction> TransactionQueue::moveVerifiedTrxSnapShot(uint16_t max_trx_to_pack) {
  std::unordered_map<trx_hash_t, Transaction> res;
  {
    uLock lock(main_shared_mutex_);
    if (max_trx_to_pack == 0) {
      for (auto const &trx : verified_trxs_) {
        res[trx.first] = *(trx.second);
      }
      verified_trxs_.clear();
    } else {
      auto it = verified_trxs_.begin();
      uint16_t counter = 0;
      while (it != verified_trxs_.end() && max_trx_to_pack != counter) {
        res[it->first] = *(it->second);
        it = verified_trxs_.erase(it);
        counter++;
      }
    }
    for (auto const &t : res) {
      trx_buffer_.erase(queued_trxs_[t.first]);
      queued_trxs_.erase(t.first);
    }
  }
  if (res.size() > 0) {
    LOG(log_dg_) << "Copy " << res.size() << " verified trx. " << std::endl;
  }
  return res;
}

unsigned long TransactionQueue::getVerifiedTrxCount() const {
  sharedLock lock(main_shared_mutex_);
  return verified_trxs_.size();
}

std::pair<size_t, size_t> TransactionQueue::getTransactionQueueSize() const {
  std::pair<size_t, size_t> res;
  {
    sharedLock lock(shared_mutex_for_unverified_qu_);
    res.first = unverified_hash_qu_.size();
  }
  {
    sharedLock lock(main_shared_mutex_);
    res.second = verified_trxs_.size();
  }
  return res;
}

}  // namespace taraxa
