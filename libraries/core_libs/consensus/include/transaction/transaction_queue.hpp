#pragma once

#include "common/constants.hpp"
#include "common/util.hpp"
#include "transaction/transaction.hpp"

namespace taraxa {

/** @addtogroup Transaction
 * @{
 */

enum class TransactionStatus;
namespace final_chain {
class FinalChain;
}
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
  TransactionQueue(std::shared_ptr<final_chain::FinalChain> final_chain, size_t max_size = kMinTransactionPoolSize);

  /**
   * @brief insert a transaction into the queue, sorted by priority
   *
   * @param transaction
   * @param last_block_number
   * @return TransactionStatus
   */
  TransactionStatus insert(std::shared_ptr<Transaction>&& transaction, bool proposable, uint64_t last_block_number = 0);

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
   * @brief returns up to the number of requested transaction sorted by priority. This call actually sorts the
   * transaction so it is expensive and should be used only when needed
   *
   * @param count
   * @return std::vector<std::shared_ptr<Transaction>>
   */
  std::vector<std::shared_ptr<Transaction>> getOrderedTransactions(uint64_t count,
                                                                   std::optional<PbftPeriod> period = {}) const;

  /**
   * @brief returns all transactions grouped by transactions author
   *
   * @return std::vector<SharedTransactions>
   */
  std::vector<SharedTransactions> getAllTransactions() const;

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
   * @brief Removes any transactions that are no longer proposable
   *
   * @param final_chain
   */
  void purge();

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

  /**
   * @brief Have transactions been recently dropped due to queue reaching max size
   * This call is thread-safe
   * @return Returns true if txs were dropped
   */
  bool transactionsDropped() const {
    return std::chrono::system_clock::now() - transaction_overflow_time_ < kTransactionOverflowTimeLimit;
  }

  /**
   * @brief return true if non proposable transactions are over the limit
   *
   * @return true
   * @return false
   */
  bool nonProposableTransactionsOverTheLimit() const;

 private:
  // Transactions in the queue per account ordered by nonce
  std::unordered_map<addr_t, std::map<val_t, std::shared_ptr<Transaction>>> account_nonce_transactions_;

  // Transactions in the queue per trx hash
  std::unordered_map<trx_hash_t, std::shared_ptr<Transaction>> queue_transactions_;

  // Low nonce and insufficient balance transactions which should not be included in proposed dag blocks but it is
  // possible because of dag reordering that some dag block might arrive requiring these transactions.
  std::unordered_map<trx_hash_t, std::pair<uint64_t, std::shared_ptr<Transaction>>> non_proposable_transactions_;

  ExpirationCache<trx_hash_t> known_txs_;

  // Last time transactions were dropped due to queue reaching max size
  std::chrono::system_clock::time_point transaction_overflow_time_;

  // If transactions are dropped within last kTransactionOverflowTimeLimit seconds, dag blocks with missing transactions
  // will not be treated as malicious
  const std::chrono::seconds kTransactionOverflowTimeLimit{600};

  // Limit when non proposable transactions expire
  const size_t kNonProposableTransactionsPeriodExpiryLimit = 10;

  // Maximum number of non proposable transactions in percentage of kMaxSize
  const size_t kNonProposableTransactionsLimitPercentage = 20;

  // Maximum number of single account transactions in percentage of kMaxSize
  const size_t kSingleAccountTransactionsLimitPercentage = 5;

  // Maximum number of non proposable transactions
  const size_t kNonProposableTransactionsMaxSize;

  // Maximum size of transactions pool
  const size_t kMaxSize;

  // Maximum size of single account transactions
  const size_t kMaxSingleAccountTransactionsSize;

  std::shared_ptr<final_chain::FinalChain> final_chain_;
};

/** @}*/

}  // namespace taraxa