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
  bool insert(std::shared_ptr<Transaction>&& transaction);

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
  typedef std::function<bool(const std::shared_ptr<Transaction>&, const std::shared_ptr<Transaction>&)> ComperType;
  // It has to be multiset as two trx could have same value (nonce and gas price)
  using PriorityQueue = std::multiset<std::shared_ptr<Transaction>, ComperType>;
  PriorityQueue priority_queue_;
  std::unordered_map<trx_hash_t, PriorityQueue::iterator> hash_queue_;
};

}  // namespace taraxa