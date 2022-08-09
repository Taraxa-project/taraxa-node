#pragma once

#include "common/constants.hpp"
#include "common/util.hpp"
#include "transaction/transaction.hpp"

namespace taraxa {

/** @addtogroup Transaction
 * @{
 */

enum class TransactionStatus;
/**
 * @brief TransactionQueue keeps transactions in memory sorted by priority to be included in a proposed DAG block or
 * which might be required by an incoming DAG block
 *
 * TransactionQueue stores two type of transactions: Transactions that can be included in a proposed DAG block and non
 * proposable transactions which are not to be used for proposal but an incoming DAG block could contain such
 * transactions. Non proposable transactions can expire if no DAG block that contains them is received within the
 * kNonProposableTransactionsPeriodExpiryLimit.
 *
 * This is NOT thread safe class. It is proteced only by transactions_mutex_
 * in the TransactionsManager !!!
 *
 */
class TransactionQueue {
 public:
  TransactionQueue(size_t max_size = kMinTransactionPoolSize);

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

  /**
   * @brief Marks transaction as known (was successfully pushed into the tx pool)
   * This call is thread-safe
   * @param trx_hash transaction hash
   */
  void markTransactionKnown(const trx_hash_t& trx_hash);

  /**
   * @param trx_hash transaction hash
   * This call is thread-safe
   * @return Returns true if tx is known (was successfully pushed into the tx pool)
   */
  bool isTransactionKnown(const trx_hash_t& trx_hash) const;

 private:
  typedef std::function<bool(const std::shared_ptr<Transaction>&, const std::shared_ptr<Transaction>&)> ComperType;
  // It has to be multiset as two trx could have same value (nonce and gas price)
  using PriorityQueue = std::multiset<std::shared_ptr<Transaction>, ComperType>;
  PriorityQueue priority_queue_;
  std::unordered_map<trx_hash_t, PriorityQueue::iterator> hash_queue_;
  // Low nonce and insufficient balance transactions which should not be included in proposed dag blocks but it is
  // possible because of dag reordering that some dag block might arrive requiring these transactions.
  std::unordered_map<trx_hash_t, std::pair<uint64_t, std::shared_ptr<Transaction>>> non_proposable_transactions_;

  ExpirationCache<trx_hash_t> known_txs_;

  // Limit when non proposable transactions expire
  const size_t kNonProposableTransactionsPeriodExpiryLimit = 10;

  // Maximum number of save transactions
  const size_t kNonProposableTransactionsLimit = 1000;

  // Maximum size of transactions pool
  const size_t kMaxSize;
};

/** @}*/

}  // namespace taraxa