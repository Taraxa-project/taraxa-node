#include "transaction/transaction_queue.hpp"

#include "transaction/transaction_manager.hpp"

namespace taraxa {

auto priorityComparator = [](const std::shared_ptr<Transaction> &first, const std::shared_ptr<Transaction> &second) {
  if (first->getSender() == second->getSender()) {
    return first->getNonce() < second->getNonce() ||
           (first->getNonce() == second->getNonce() && first->getGasPrice() > second->getGasPrice());
  } else {
    return first->getGasPrice() > second->getGasPrice();
  }
};

TransactionQueue::TransactionQueue(size_t max_size)
    : priority_queue_{priorityComparator}, known_txs_(max_size * 2, max_size / 5), kMaxSize(max_size) {
  // There are library limits on multiset size, we need to check if max size is not exceeding it
  if (kMaxSize > priority_queue_.max_size()) {
    throw std::invalid_argument("Transaction pool size is too large");
  }
}

size_t TransactionQueue::size() const { return hash_queue_.size(); }

bool TransactionQueue::contains(const trx_hash_t &hash) const {
  return hash_queue_.contains(hash) || non_proposable_transactions_.contains(hash);
}

std::shared_ptr<Transaction> TransactionQueue::get(const trx_hash_t &hash) const {
  if (const auto it = hash_queue_.find(hash); it != hash_queue_.end()) {
    return *(it->second);
  }

  if (const auto transaction = non_proposable_transactions_.find(hash);
      transaction != non_proposable_transactions_.end()) {
    return transaction->second.second;
  }

  return nullptr;
}

std::vector<std::shared_ptr<Transaction>> TransactionQueue::get(uint64_t count) const {
  if (count == 0 || priority_queue_.size() <= count) {
    std::vector<std::shared_ptr<Transaction>> ret(priority_queue_.begin(), priority_queue_.end());
    return ret;
  }

  std::vector<std::shared_ptr<Transaction>> ret;
  ret.reserve(count);
  size_t counter = 0;
  for (const auto &trx : priority_queue_) {
    if (counter == count) break;
    ret.push_back(trx);
    counter++;
  }
  return ret;
}

bool TransactionQueue::erase(const trx_hash_t &hash) {
  // Find the hash
  const auto it = hash_queue_.find(hash);
  if (it == hash_queue_.end()) return false;

  // Clean the rest
  priority_queue_.erase(it->second);
  hash_queue_.erase(it);
  return true;
}

bool TransactionQueue::insert(std::pair<std::shared_ptr<Transaction>, TransactionStatus> &&transaction,
                              uint64_t last_block_number) {
  assert(transaction.first);
  const auto tx_hash = transaction.first->getHash();

  if (contains(tx_hash)) return false;

  switch (transaction.second) {
    case TransactionStatus::Verified: {
      const auto it = priority_queue_.insert(std::move(transaction.first));

      // This assert is here to check if priorityComparator works correctly. If object is not inserted, then there could
      // be something wrong with comparator
      assert(it != priority_queue_.end());

      // This check if priority_queue_ is not bigger than max size if so we delete last object
      // if the last object is also current one we return false
      if (priority_queue_.size() > kMaxSize) [[unlikely]] {
        const auto last_el = std::prev(priority_queue_.end());
        if (it == last_el) {
          priority_queue_.erase(last_el);
          return false;
        }
        known_txs_.erase((*last_el)->getHash());
        hash_queue_.erase((*last_el)->getHash());
        priority_queue_.erase(last_el);
      }
      hash_queue_[tx_hash] = it;
      known_txs_.insert(tx_hash);
    } break;
    case TransactionStatus::LowNonce:
    case TransactionStatus::InsufficentBalance:
    case TransactionStatus::Forced:
      if (non_proposable_transactions_.size() < kNonProposableTransactionsLimit) {
        non_proposable_transactions_[tx_hash] = {last_block_number, transaction.first};
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