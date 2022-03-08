#include "transaction/transaction_queue.hpp"

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

bool TransactionQueue::contains(const trx_hash_t &hash) const { return hash_queue_.contains(hash); }

std::shared_ptr<Transaction> TransactionQueue::get(const trx_hash_t &hash) const {
  const auto it = hash_queue_.find(hash);
  if (it == hash_queue_.end()) return nullptr;

  return *(it->second);
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

bool TransactionQueue::insert(std::shared_ptr<Transaction> &&transaction) {
  assert(transaction);
  if (hash_queue_.contains(transaction->getHash())) return false;

  const auto tx_hash = transaction->getHash();
  const auto it = priority_queue_.insert(std::move(transaction));

  // This assert is here to check if priorityComparator works correctly. If object is not inserted, then there could be
  // something wrong with comparator
  assert(it != priority_queue_.end());
  hash_queue_[tx_hash] = it;
  return true;
}

}  // namespace taraxa