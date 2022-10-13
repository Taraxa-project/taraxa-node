#include "transaction/transaction_queue.hpp"

#include "transaction/transaction_manager.hpp"

namespace taraxa {

TransactionQueue::TransactionQueue(size_t max_size) : known_txs_(max_size * 2, max_size / 5), kMaxSize(max_size) {}

size_t TransactionQueue::size() const { return queue_transactions_.size(); }

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

  std::multimap<val_t, std::shared_ptr<Transaction>, std::greater<val_t>> head_transactions;
  std::unordered_map<addr_t, std::pair<std::map<val_t, std::shared_ptr<Transaction>>::const_iterator,
                                       std::map<val_t, std::shared_ptr<Transaction>>::const_iterator>>
      iterators;

  // For accounts with multiple transactions we will iterate one level at a time
  for (const auto &account : account_nonce_transactions_) {
    iterators.insert({account.first, {account.second.begin(), account.second.end()}});
  }

  for (auto it = iterators.begin(); it != iterators.end(); it++) {
    head_transactions.insert({it->second.first->second->getGasPrice(), it->second.first->second});
    // Increase iterator for an account
    it->second.first++;
  }
  while (true) {
    // Take transactions with highest gas and put it in ordered transactions
    auto head_trx = head_transactions.begin();
    ret.push_back(head_trx->second);
    // If there is next nonce transaction of same account put it in head transactions
    auto &it = iterators[head_trx->second->getSender()];
    head_transactions.erase(head_trx);
    if (it.first != it.second) {
      head_transactions.insert({it.first->second->getGasPrice(), it.first->second});
      it.first++;
    }
    if (head_transactions.empty()) {
      break;
    }
  }

  return ret;
}

SharedTransactions TransactionQueue::getAllTransactions() const {
  SharedTransactions ret;
  ret.reserve(queue_transactions_.size());
  for (const auto &t : queue_transactions_) ret.push_back(t.second);
  return ret;
}

bool TransactionQueue::erase(const trx_hash_t &hash) {
  // Find the hash
  const auto it = queue_transactions_.find(hash);
  if (it == queue_transactions_.end()) return false;
  queue_transactions_.erase(it);

  const auto &account_it = account_nonce_transactions_.find(it->second->getSender());
  if (account_it == account_nonce_transactions_.end()) {
    return false;
  }
  const auto &nonce_it = account_it->second.find(it->second->getNonce());
  if (nonce_it == account_it->second.end()) {
    return false;
  }

  if (hash == nonce_it->second->getHash()) {
    account_it->second.erase(nonce_it);
    if (account_it->second.size() == 0) {
      account_nonce_transactions_.erase(account_it);
    }
  }

  return true;
}

bool TransactionQueue::insert(std::shared_ptr<Transaction> &&transaction, const TransactionStatus status,
                              uint64_t last_block_number) {
  assert(transaction);
  const auto tx_hash = transaction->getHash();

  if (contains(tx_hash)) return false;

  switch (status) {
    case TransactionStatus::Verified: {
      const auto &account_it = account_nonce_transactions_.find(transaction->getSender());
      if (account_it == account_nonce_transactions_.end()) {
        account_nonce_transactions_[transaction->getSender()][transaction->getNonce()] = transaction;
        queue_transactions_[tx_hash] = transaction;
      } else {
        const auto &nonce_it = account_it->second.find(transaction->getNonce());
        if (nonce_it == account_it->second.end()) {
          account_nonce_transactions_[transaction->getSender()][transaction->getNonce()] = transaction;
          queue_transactions_[tx_hash] = transaction;
        } else {
          // It should not be possible that transaction is already inside due to verification done before
          assert(nonce_it->second->getHash() != tx_hash);
          // Replace transaction if gas price higher
          if (transaction->getGasPrice() > nonce_it->second->getGasPrice()) {
            // Place same nonce transaction with lower gas price in non propsable transactions since it could be
            // possible that some dag block might contain it
            non_proposable_transactions_[nonce_it->second->getHash()] = {last_block_number, nonce_it->second};
            queue_transactions_.erase(nonce_it->second->getHash());
            account_nonce_transactions_[transaction->getSender()][transaction->getNonce()] = transaction;
            queue_transactions_[tx_hash] = transaction;
          }
        }
      }

      auto queue_size = size();
      // This check if priority_queue_ is not bigger than max size if so we delete 1% of transactions
      if (size() > kMaxSize) [[unlikely]] {
        auto ordered_transactions = getOrderedTransactions(queue_size);
        uint32_t counter = 0;
        for (auto it = ordered_transactions.rbegin(); it != ordered_transactions.rend(); it++) {
          erase((*it)->getHash());
          known_txs_.erase((*it)->getHash());
          counter++;
          if (counter >= queue_size / 100) break;
        }
        if (!queue_transactions_.contains(tx_hash)) {
          return false;
        }
      }
      known_txs_.insert(tx_hash);
    } break;
    case TransactionStatus::LowNonce:
    case TransactionStatus::InsufficentBalance:
    case TransactionStatus::Forced:
      if (non_proposable_transactions_.size() < kNonProposableTransactionsLimit) {
        non_proposable_transactions_[tx_hash] = {last_block_number, transaction};
        known_txs_.insert(tx_hash);
      } else {
        return false;
      }
      break;
    default:
      assert(false);
  }
  return true;
}

void TransactionQueue::blockFinalized(uint64_t block_number) {
  for (auto it = non_proposable_transactions_.begin(); it != non_proposable_transactions_.end();) {
    if (it->second.first + kNonProposableTransactionsPeriodExpiryLimit < block_number) {
      it = non_proposable_transactions_.erase(it);
    } else {
      ++it;
    }
  }
}

void TransactionQueue::markTransactionKnown(const trx_hash_t &trx_hash) { known_txs_.insert(trx_hash); }

bool TransactionQueue::isTransactionKnown(const trx_hash_t &trx_hash) const { return known_txs_.contains(trx_hash); }

}  // namespace taraxa