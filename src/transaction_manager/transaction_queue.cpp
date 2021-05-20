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
    uLock lock(shared_mutex_for_queued_trxs_);
    iter = trx_buffer_.insert(trx_buffer_.end(), trx);
    assert(iter != trx_buffer_.end());
    queued_trxs_[trx.getHash()] = iter;
  }
  if (verify) {
    uLock lock(shared_mutex_for_verified_qu_);
    verified_trxs_[trx.getHash()] = iter;
    new_verified_transactions_ = true;
  } else {
    uLock lock(shared_mutex_for_unverified_qu_);
    unverified_hash_qu_.emplace_back(std::make_pair(hash, iter));
    cond_for_unverified_qu_.notify_one();
  }
  LOG(log_nf_) << " Trx: " << hash << " inserted. " << verify << std::endl;
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
  uLock lock(shared_mutex_for_queued_trxs_);
  trx_buffer_.erase(queued_trxs_[hash]);
  queued_trxs_.erase(hash);
}

void TransactionQueue::addTransactionToVerifiedQueue(trx_hash_t const &hash, std::list<Transaction>::iterator iter) {
  uLock lock(shared_mutex_for_verified_qu_);
  verified_trxs_[hash] = iter;
  new_verified_transactions_ = true;
}

// The caller is responsible for storing the transaction to db!
std::unordered_map<trx_hash_t, Transaction> TransactionQueue::removeBlockTransactionsFromQueue(
    vec_trx_t const &all_block_trxs) {
  std::unordered_map<trx_hash_t, Transaction> result;
  {
    std::vector<trx_hash_t> removed_trx;
    for (auto const &trx : all_block_trxs) {
      {
        upgradableLock lock(shared_mutex_for_verified_qu_);
        auto vrf_trx = verified_trxs_.find(trx);
        if (vrf_trx != verified_trxs_.end()) {
          result[vrf_trx->first] = *(vrf_trx->second);
          removed_trx.emplace_back(trx);
          {
            upgradeLock locked(lock);
            verified_trxs_.erase(trx);
          }
        }
      }
    }
    // TODO: can also remove from unverified_hash_qu_

    // clear trx_buffer
    {
      uLock lock(shared_mutex_for_queued_trxs_);
      for (auto const &t : removed_trx) {
        trx_buffer_.erase(queued_trxs_[t]);
        queued_trxs_.erase(t);
      }
    }
  }
  return result;
}

std::unordered_map<trx_hash_t, Transaction> TransactionQueue::getVerifiedTrxSnapShot() const {
  std::unordered_map<trx_hash_t, Transaction> verified_trxs;
  sharedLock lock(shared_mutex_for_verified_qu_);
  for (auto const &trx : verified_trxs_) {
    verified_trxs[trx.first] = *(trx.second);
  }
  LOG(log_dg_) << "Get: " << verified_trxs.size() << " verified trx out. " << std::endl;
  return verified_trxs;
}

std::vector<Transaction> TransactionQueue::getNewVerifiedTrxSnapShot() {
  std::vector<Transaction> verified_trxs;
  sharedLock lock(shared_mutex_for_verified_qu_);
  {
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
  {
    sharedLock lock(shared_mutex_for_queued_trxs_);
    auto it = queued_trxs_.find(hash);
    if (it != queued_trxs_.end()) {
      return std::make_shared<Transaction>(*(it->second));
    }
  }
  return nullptr;
}

std::unordered_map<trx_hash_t, Transaction> TransactionQueue::moveVerifiedTrxSnapShot(uint16_t max_trx_to_pack) {
  std::unordered_map<trx_hash_t, Transaction> res;
  {
    upgradableLock lock(shared_mutex_for_verified_qu_);
    if (max_trx_to_pack == 0) {
      for (auto const &trx : verified_trxs_) {
        res[trx.first] = *(trx.second);
      }
      upgradeLock locked(lock);
      verified_trxs_.clear();
    } else {
      auto it = verified_trxs_.begin();
      uint16_t counter = 0;
      while (it != verified_trxs_.end() && max_trx_to_pack != counter) {
        res[it->first] = *(it->second);
        upgradeLock locked(lock);
        it = verified_trxs_.erase(it);
        counter++;
      }
    }
  }
  {
    uLock lock(shared_mutex_for_queued_trxs_);
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
  sharedLock lock(shared_mutex_for_verified_qu_);
  return verified_trxs_.size();
}

std::pair<size_t, size_t> TransactionQueue::getTransactionQueueSize() const {
  std::pair<size_t, size_t> res;
  {
    sharedLock lock(shared_mutex_for_unverified_qu_);
    res.first = unverified_hash_qu_.size();
  }
  {
    sharedLock lock(shared_mutex_for_verified_qu_);
    res.second = verified_trxs_.size();
  }
  return res;
}

}  // namespace taraxa
