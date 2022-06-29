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

TransactionQueue::TransactionQueue() : priority_queue_{priorityComparator} {}

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

void TransactionQueue::resetGetNextTransactionIterator() { priority_queue_it_ = priority_queue_.begin(); }

std::shared_ptr<Transaction> TransactionQueue::getNextTransaction() {
  if (priority_queue_it_ == priority_queue_.end()) return nullptr;
  auto res = *priority_queue_it_;
  priority_queue_it_++;
  return res;
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
      hash_queue_[tx_hash] = it;
    } break;
    case TransactionStatus::LowNonce:
    case TransactionStatus::InsufficentBalance:
      if (non_proposable_transactions_.size() < kNonProposableTransactionsLimit)
        non_proposable_transactions_[tx_hash] = {last_block_number, transaction.first};
      else
        return false;
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

}  // namespace taraxa