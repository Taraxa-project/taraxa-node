#include "transaction_manager/transaction_queue.hpp"

namespace taraxa {

TransactionQueue::TransactionQueue() : priority_queue_{PriorityCompare{*this}} {}

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

  // Remove trx from nonce map
  const auto nonce_it = nonce_queue_.find((*it->second)->getSender());
  nonce_it->second.erase((*it->second)->getNonce());
  // Remove even address from nonce map
  if (nonce_it->second.empty()) nonce_queue_.erase(nonce_it);

  // Clean the rest
  priority_queue_.erase(it->second);
  hash_queue_.erase(it);
  return true;
}

bool TransactionQueue::insert(const std::shared_ptr<Transaction> &transaction) {
  assert(transaction);
  if (hash_queue_.contains(transaction->getHash())) return false;
  // First we need to insert it in nonce map, so compare works fine
  auto nonce_it = nonce_queue_[transaction->getSender()].emplace(
      std::make_pair(transaction->getNonce(), PriorityQueue::iterator()));
  const auto it = priority_queue_.insert(transaction);
  // This assert is here to check if PriorityCompare works correctly. If object is not inserted, then there could be
  // something wrong with comparator
  assert(it != priority_queue_.end());
  nonce_it.first->second = it;
  hash_queue_[transaction->getHash()] = it;
  return true;
}

}  // namespace taraxa