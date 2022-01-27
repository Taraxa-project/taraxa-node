#pragma once

#include "transaction/transaction.hpp"

namespace taraxa {

class TransactionQueue {
 public:
  TransactionQueue();

  bool insert(const std::shared_ptr<Transaction>& transaction);
  bool erase(const trx_hash_t& hash);

  std::shared_ptr<Transaction> get(const trx_hash_t& hash) const;
  std::vector<std::shared_ptr<Transaction>> get(uint64_t count = 0) const;
  bool contains(const trx_hash_t& hash) const;
  size_t size() const;

 private:
  struct PriorityCompare {
    TransactionQueue& queue;
    /// Compare transaction by nonce height and gas price.
    bool operator()(const std::shared_ptr<Transaction>& first, const std::shared_ptr<Transaction>& second) const {
      const auto& height1 = first->getNonce() - queue.nonce_queue_[first->getSender()].begin()->first;
      const auto& height2 = second->getNonce() - queue.nonce_queue_[second->getSender()].begin()->first;
      return height1 < height2 || (height1 == height2 && first->getGasPrice() > second->getGasPrice());
    }
  };
  // It has to be multiset as two trx could have same value (nonce and gas price)
  using PriorityQueue = std::multiset<std::shared_ptr<Transaction>, PriorityCompare>;
  PriorityQueue prio_queue_;
  std::unordered_map<trx_hash_t, PriorityQueue::iterator> hash_queue_;
  std::unordered_map<addr_t, std::map<u256, PriorityQueue::iterator>> nonce_queue_;
};

}  // namespace taraxa