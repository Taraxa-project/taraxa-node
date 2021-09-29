#include "transaction_manager/transaction_queue.hpp"

#include <string>
#include <utility>

#include "storage/storage.hpp"
#include "transaction/status.hpp"
#include "transaction/transaction.hpp"

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

bool TransactionQueue::insert(Transaction const &trx, bool queue_verified, bool tx_verified,
                              const std::shared_ptr<DbStorage> &db) {
  const auto hash = trx.getHash();
  auto trx_ptr = std::make_shared<Transaction>(trx);

  {
    uLock lock(main_shared_mutex_);

    // Transaction is already in queued_trxs_, do not insert it again.
    if (!queued_trxs_.emplace(hash, trx_ptr).second) {
      LOG(log_nf_) << "Tx " << hash.abridged() << " already inserted, skip it";
      return false;
    }

    TransactionStatusEnum tx_status = TransactionStatusEnum::in_queue_unverified;

    // shared_mutex_for_unverified_qu_ moved out of "if (queue_verified)" else branch so the lock is released
    // after db operations are performed. Otherwise there is race condition
    uLock unlock(shared_mutex_for_unverified_qu_);
    if (queue_verified) {
      verified_trxs_[hash] = std::move(trx_ptr);
      new_verified_transactions_ = true;
      tx_status = TransactionStatusEnum::in_queue_verified;
    } else {
      unverified_hash_qu_.emplace_back(std::make_pair(hash, std::move(trx_ptr)));
      cond_for_unverified_qu_.notify_one();
    }

    db->saveTransaction(trx, tx_verified);
    db->saveTransactionStatus(hash, tx_status);
  }

  LOG(log_nf_) << " Tx " << hash.abridged() << " inserted. Verification processed: " << tx_verified;
  return true;
}

void TransactionQueue::insertUnverifiedTrxs(const std::vector<Transaction> &trxs,
                                            const std::shared_ptr<DbStorage> &db) {
  if (trxs.empty()) {
    return;
  }

  auto write_batch = db->createWriteBatch();

  uLock lock(main_shared_mutex_);
  uLock unverified_lock(shared_mutex_for_unverified_qu_);
  for (const auto &trx : trxs) {
    const auto tx_hash = trx.getHash();
    // TODO: get rid of this useless Transaction copy
    auto trx_ptr = std::make_shared<Transaction>(trx);

    // There might be race condition here but it is not because of the way we process txs in queued_trxs_
    // Important: in case txs processing in this queue is somehow changed, this race condition must be handled !!!

    // Lets say there 2 packets with the same tx in priority queue. First thread locks main_shared_mutex_ and
    // push tx into unverified queue + db.
    // Second thread then polls this tx from unverified queue(locks main_shared_mutex_) to verify it.
    // Third thread starts then process the same tx as first thread already processed and because it is not in
    // unverified queue anymore it might push it there again + modiffies db
    // But because we check queued_trxs_ and not unverified_hash_qu_ + queued_trxs_ is cleared only after
    // new block is applied, such situation should not happen
    if (!queued_trxs_.emplace(tx_hash, trx_ptr).second) {
      LOG(log_wr_) << "insertUnverifiedTrxs: Tx " << tx_hash << " already inserted, skip it";
      continue;
    }

    db->addTransactionToBatch(trx, write_batch);
    db->addTransactionStatusToBatch(write_batch, tx_hash, TransactionStatusEnum::in_queue_unverified);

    unverified_hash_qu_.emplace_back(std::make_pair(tx_hash, std::move(trx_ptr)));
    cond_for_unverified_qu_.notify_one();
    LOG(log_nf_) << "Tx: " << tx_hash << " inserted";
  }

  db->commitWriteBatch(write_batch);
}

std::pair<trx_hash_t, std::shared_ptr<Transaction>> TransactionQueue::getUnverifiedTransaction() {
  std::pair<trx_hash_t, std::shared_ptr<Transaction>> item;
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
  queued_trxs_.erase(hash);
}

void TransactionQueue::addTransactionToVerifiedQueue(trx_hash_t const &hash, std::shared_ptr<Transaction> trx) {
  uLock verify_lock(main_shared_mutex_);
  // This function is called from several background threads by verifyQueuedTrxs()
  // and at the same time verifyBlockTransactions() can be called, that is removing
  // all trxs that are already in DAG block from those queues. So we need to have lock here
  // and check if the transaction was not removed by DAG block
  if (queued_trxs_.find(hash) == queued_trxs_.end()) {
    return;
  }

  verified_trxs_[hash] = trx;
  new_verified_transactions_ = true;
}

// The caller is responsible for storing the transaction to db!
void TransactionQueue::removeBlockTransactionsFromQueue(vec_trx_t const &all_block_trxs) {
  uLock lock(main_shared_mutex_);
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
    uLock unveried_lock(shared_mutex_for_unverified_qu_);
    unverified_hash_qu_.erase(std::remove_if(unverified_hash_qu_.begin(), unverified_hash_qu_.end(),
                                             [&unverif_to_remove](const auto &el) {
                                               return unverif_to_remove.find(el.first) != unverif_to_remove.end();
                                             }),
                              unverified_hash_qu_.end());
  }
  // clear trx_buffer
  for (auto const &t : to_remove) {
    queued_trxs_.erase(t);
  }
}

std::vector<Transaction> TransactionQueue::getNewVerifiedTrxSnapShot() {
  std::vector<Transaction> verified_trxs;
  {
    sharedLock lock(main_shared_mutex_);
    if (new_verified_transactions_) {
      verified_trxs.reserve(verified_trxs_.size());
      new_verified_transactions_ = false;
      std::transform(
          verified_trxs_.begin(), verified_trxs_.end(), std::back_inserter(verified_trxs),
          [](std::pair<const taraxa::trx_hash_t, std::shared_ptr<Transaction>> const &trx) { return *(trx.second); });
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
    sharedLock lock(main_shared_mutex_);
    res.second = verified_trxs_.size();
  }
  {
    sharedLock lock(shared_mutex_for_unverified_qu_);
    res.first = unverified_hash_qu_.size();
  }
  return res;
}

size_t TransactionQueue::getTransactionBufferSize() const {
  sharedLock lock(main_shared_mutex_);
  return queued_trxs_.size();
}

}  // namespace taraxa
