#pragma once

#include "transaction/transaction.hpp"

namespace taraxa {
/**
 * @brief this is NOT thread safe class. It is proteced only by transactions_mutex_ in the transaction mamanger !!!
 *
 */
class TransactionQueue {
 public:
  TransactionQueue();

  /**
   * @brief insert a transaction into the queue, sorted by priority
   *
   * @param transaction
   * @return true
   * @return false
   */
  bool insert(const std::shared_ptr<Transaction>& transaction);

  /**
   * @brief remove transaction from queue
   *
   * @param hash
   * @return true
   * @return false
   */
  bool erase(const trx_hash_t& hash);

  /**
   * @brief returns the transaction or null
   *
   * @param hash
   * @return std::shared_ptr<Transaction>
   */
  std::shared_ptr<Transaction> get(const trx_hash_t& hash) const;

  /**
   * @brief returns up to the number of requested transaction sorted by priority
   *
   * @param count
   * @return std::vector<std::shared_ptr<Transaction>>
   */
  std::vector<std::shared_ptr<Transaction>> get(uint64_t count = 0) const;

  /**
   * @brief returns true/false if the transaction is in the queue
   *
   * @param hash
   * @return true
   * @return false
   */
  bool contains(const trx_hash_t& hash) const;

  /**
   * @brief returns size of priority queue
   *
   * @return size_t
   */
  size_t size() const;

 private:
  struct PriorityCompare {
    TransactionQueue& queue;
    // Compare transaction by nonce height and gas price.
    bool operator()(const std::shared_ptr<Transaction>& first, const std::shared_ptr<Transaction>& second) const {
      const auto& height1 = first->getNonce() - queue.nonce_queue_[first->getSender()].begin()->first;
      const auto& height2 = second->getNonce() - queue.nonce_queue_[second->getSender()].begin()->first;
      if (first->getSender() == second->getSender()) {
        return height1 < height2 || (height1 == height2 && first->getGasPrice() > second->getGasPrice());
      } else {
        return first->getGasPrice() > second->getGasPrice();
      }
    }
  };
  // It has to be multiset as two trx could have same value (nonce and gas price)
  using PriorityQueue = std::multiset<std::shared_ptr<Transaction>, PriorityCompare>;
  PriorityQueue priority_queue_;
  std::unordered_map<trx_hash_t, PriorityQueue::iterator> hash_queue_;
  std::unordered_map<addr_t, std::map<u256, PriorityQueue::iterator>> nonce_queue_;
};

}  // namespace taraxa