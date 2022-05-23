#pragma once

#include "transaction/transaction.hpp"

namespace taraxa {
enum class TransactionStatus;
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
   * @param last_block_number
   * @return true
   * @return false
   */
  bool insert(std::pair<std::shared_ptr<Transaction>, TransactionStatus>&& transaction, uint64_t last_block_number = 0);

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

  /**
   * @brief Invoked when block finalized in final chain
   *
   * @param block_number block number finalized
   */
  void blockFinalized(uint64_t block_number);

 private:
  typedef std::function<bool(const std::shared_ptr<Transaction>&, const std::shared_ptr<Transaction>&)> ComperType;
  // It has to be multiset as two trx could have same value (nonce and gas price)
  using PriorityQueue = std::multiset<std::shared_ptr<Transaction>, ComperType>;
  PriorityQueue priority_queue_;
  std::unordered_map<trx_hash_t, PriorityQueue::iterator> hash_queue_;
  // Low nonce and insufficient balance transactions which should not be included in proposed dag blocks but it is
  // possible because of dag reordering that some dag block might arrive requiring these transactions.
  std::unordered_map<trx_hash_t, std::pair<uint64_t, std::shared_ptr<Transaction>>> non_proposable_transactions_;

  // Limit when non proposable transactions expire
  const uint64_t kNonProposableTransactionsPeriodExpiryLimit = 10;

  // Maximum number of save transactions
  const uint64_t kNonProposableTransactionsLimit = 1000;
};

}  // namespace taraxa