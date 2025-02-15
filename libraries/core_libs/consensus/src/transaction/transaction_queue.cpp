#include "transaction/transaction_queue.hpp"

#include "transaction/transaction_manager.hpp"

namespace taraxa {

TransactionQueue::TransactionQueue(std::shared_ptr<final_chain::FinalChain> final_chain, size_t max_size)
    : known_txs_(max_size * 2, max_size / 5),
      kNonProposableTransactionsMaxSize(max_size * kNonProposableTransactionsLimitPercentage / 100),
      kMaxSize(max_size),
      kMaxDataSize(max_size * 1024),  // Data limit is max_size kB
      kMaxSingleAccountTransactionsSize(max_size * kSingleAccountTransactionsLimitPercentage / 100),
      final_chain_(final_chain) {
  queue_transactions_.reserve(max_size);
}

size_t TransactionQueue::size() const { return queue_transactions_.size(); }

void TransactionQueue::addTransaction(const SharedTransaction &transaction, bool proposable,
                                      uint64_t last_block_number) {
  if (proposable) {
    if (queue_transactions_.emplace(transaction->getHash(), transaction).second) {
      data_size_ += transaction->getData().size();
    }
  } else {
    if (non_proposable_transactions_
            .emplace(transaction->getHash(), std::pair<uint64_t, SharedTransaction>{last_block_number, transaction})
            .second) {
      data_size_ += transaction->getData().size();
    }
  }
}

bool TransactionQueue::removeTransaction(const SharedTransaction &transaction, bool proposable) {
  if (proposable) {
    if (queue_transactions_.erase(transaction->getHash()) > 0) {
      data_size_ -= transaction->getData().size();
      return true;
    }
  } else {
    if (non_proposable_transactions_.erase(transaction->getHash()) > 0) {
      data_size_ -= transaction->getData().size();
      return true;
    }
  }
  return false;
}

std::unordered_map<taraxa::trx_hash_t, std::pair<uint64_t, taraxa::SharedTransaction>>::iterator
TransactionQueue::removeTransaction(
    std::unordered_map<taraxa::trx_hash_t, std::pair<uint64_t, taraxa::SharedTransaction>>::iterator it) {
  data_size_ -= it->second.second->getData().size();
  return non_proposable_transactions_.erase(it);
}

bool TransactionQueue::contains(const trx_hash_t &hash) const {
  return queue_transactions_.contains(hash) || non_proposable_transactions_.contains(hash);
}

std::shared_ptr<Transaction> TransactionQueue::get(const trx_hash_t &hash) const {
  if (const auto it = queue_transactions_.find(hash); it != queue_transactions_.end()) {
    return it->second;
  }

  if (const auto transaction = non_proposable_transactions_.find(hash);
      transaction != non_proposable_transactions_.end()) {
    return transaction->second.second;
  }

  return nullptr;
}

SharedTransactions TransactionQueue::getOrderedTransactions(uint64_t count) const {
  SharedTransactions ret;
  ret.reserve(count);

  std::multimap<val_t, std::shared_ptr<Transaction>> head_transactions;
  std::unordered_map<addr_t, std::pair<std::map<val_t, std::shared_ptr<Transaction>>::const_iterator,
                                       std::map<val_t, std::shared_ptr<Transaction>>::const_iterator>>
      iterators;

  iterators.reserve(account_nonce_transactions_.size());
  // For accounts with multiple transactions we will iterate one level at a time
  for (const auto &account : account_nonce_transactions_) {
    iterators.insert({account.first, {account.second.begin(), account.second.end()}});
  }

  for (auto it = iterators.begin(); it != iterators.end(); it++) {
    head_transactions.insert({it->second.first->second->getNonce(), it->second.first->second});
    // Increase iterator for an account
    it->second.first++;
  }
  while (!head_transactions.empty()) {
    // Take transactions with lowest nonce and put it in ordered transactions
    auto head_trx = head_transactions.begin();
    ret.push_back(head_trx->second);
    if (ret.size() == count) {
      break;
    }
    // If there is next nonce transaction of same account put it in head transactions
    auto &it = iterators[head_trx->second->getSender()];
    head_transactions.erase(head_trx);
    if (it.first != it.second) {
      head_transactions.insert({it.first->second->getNonce(), it.first->second});
      it.first++;
    }
  }

  return ret;
}

std::vector<SharedTransactions> TransactionQueue::getAllTransactions() const {
  std::vector<SharedTransactions> ret;
  ret.reserve(account_nonce_transactions_.size());
  for (const auto &account_it : account_nonce_transactions_) {
    SharedTransactions trxs_per_account;
    trxs_per_account.reserve(account_it.second.size());
    for (const auto &t : account_it.second) {
      trxs_per_account.emplace_back(t.second);
    }
    ret.emplace_back(std::move(trxs_per_account));
  }
  return ret;
}

bool TransactionQueue::erase(const SharedTransaction &transaction) {
  // Find the hash
  const auto it = queue_transactions_.find(transaction->getHash());
  if (it == queue_transactions_.end()) {
    return removeTransaction(transaction, false);
  }

  const auto &account_it = account_nonce_transactions_.find(it->second->getSender());
  assert(account_it != account_nonce_transactions_.end());
  const auto &nonce_it = account_it->second.find(it->second->getNonce());
  assert(nonce_it != account_it->second.end());
  assert(transaction->getHash() == nonce_it->second->getHash());

  account_it->second.erase(nonce_it);
  if (account_it->second.size() == 0) {
    account_nonce_transactions_.erase(account_it);
  }
  return removeTransaction(transaction, true);
}

TransactionStatus TransactionQueue::insert(std::shared_ptr<Transaction> &&transaction, bool proposable,
                                           uint64_t last_block_number) {
  assert(transaction);
  const auto tx_hash = transaction->getHash();

  if (contains(tx_hash)) {
    return TransactionStatus::Known;
  }

  if (data_size_ > kMaxDataSize) {
    transaction_overflow_time_ = std::chrono::system_clock::now();
    return TransactionStatus::Overflow;
  }

  if (proposable) {
    const auto &account_it = account_nonce_transactions_.find(transaction->getSender());
    if (account_it == account_nonce_transactions_.end()) {
      account_nonce_transactions_[transaction->getSender()][transaction->getNonce()] = transaction;
      addTransaction(transaction, proposable);
    } else {
      if (account_it->second.size() == kMaxSingleAccountTransactionsSize) {
        transaction_overflow_time_ = std::chrono::system_clock::now();
        return TransactionStatus::Overflow;
      }
      const auto &nonce_it = account_it->second.find(transaction->getNonce());
      if (nonce_it == account_it->second.end()) {
        account_nonce_transactions_[transaction->getSender()][transaction->getNonce()] = transaction;
        addTransaction(transaction, proposable);
      } else {
        // It should not be possible that transaction is already inside due to verification done before
        assert(nonce_it->second->getHash() != tx_hash);
        // Replace transaction if gas price higher
        if (transaction->getGasPrice() > nonce_it->second->getGasPrice()) {
          // Place same nonce transaction with lower gas price in non proposable transactions since it could be
          // possible that some dag block might contain it
          removeTransaction(nonce_it->second, true);
          addTransaction(nonce_it->second, false, last_block_number);

          account_nonce_transactions_[transaction->getSender()][transaction->getNonce()] = transaction;
          addTransaction(transaction, proposable);
        } else {
          addTransaction(transaction, false, last_block_number);
        }
      }
    }

    const auto queue_size = size();
    // This check if priority_queue_ is not bigger than max size if so we delete 1% of transactions
    if (queue_size > kMaxSize) [[unlikely]] {
      auto ordered_transactions = getOrderedTransactions(queue_size);
      uint32_t counter = 0;
      for (auto it = ordered_transactions.rbegin(); it != ordered_transactions.rend(); it++) {
        transaction_overflow_time_ = std::chrono::system_clock::now();
        erase(*it);
        known_txs_.erase((*it)->getHash());
        counter++;
        if (counter >= queue_size / 100) break;
      }
      if (!queue_transactions_.contains(tx_hash)) {
        return TransactionStatus::Overflow;
      }
    }
    known_txs_.insert(tx_hash);
  } else {
    if (non_proposable_transactions_.size() <= kNonProposableTransactionsMaxSize) {
      addTransaction(transaction, false, last_block_number);
      known_txs_.insert(tx_hash);
      return TransactionStatus::InsertedNonProposable;
    } else {
      transaction_overflow_time_ = std::chrono::system_clock::now();
      return TransactionStatus::Overflow;
    }
  }
  return TransactionStatus::Inserted;
}

void TransactionQueue::blockFinalized(uint64_t block_number) {
  for (auto it = non_proposable_transactions_.begin(); it != non_proposable_transactions_.end();) {
    if (it->second.first + kNonProposableTransactionsPeriodExpiryLimit < block_number) {
      known_txs_.erase(it->first);
      it = removeTransaction(it);
    } else {
      ++it;
    }
  }
}

void TransactionQueue::purge() {
  for (auto account_it = account_nonce_transactions_.begin(); account_it != account_nonce_transactions_.end();) {
    const auto account = final_chain_->getAccount(account_it->first);
    if (account.has_value()) {
      for (auto nonce_it = account_it->second.begin(); nonce_it != account_it->second.end();) {
        if (nonce_it->first < account->nonce) {
          removeTransaction(nonce_it->second, true);
          nonce_it = account_it->second.erase(nonce_it);
        } else {
          break;
        }
      }

      if (account_it->second.size() == 0) {
        account_it = account_nonce_transactions_.erase(account_it);
      } else {
        account_it++;
      }
    } else {
      account_it++;
    }
  }
}

bool TransactionQueue::nonProposableTransactionsOverTheLimit() const {
  return non_proposable_transactions_.size() >= kNonProposableTransactionsMaxSize;
}

void TransactionQueue::markTransactionKnown(const trx_hash_t &trx_hash) { known_txs_.insert(trx_hash); }

bool TransactionQueue::isTransactionKnown(const trx_hash_t &trx_hash) const { return known_txs_.contains(trx_hash); }

}  // namespace taraxa