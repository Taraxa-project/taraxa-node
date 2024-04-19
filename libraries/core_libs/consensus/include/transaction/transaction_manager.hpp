#pragma once

#include "common/event.hpp"
#include "config/config.hpp"
#include "final_chain/final_chain.hpp"
#include "logger/logger.hpp"
#include "storage/storage.hpp"
#include "transaction/transaction.hpp"
#include "transaction_queue.hpp"

namespace taraxa {

/** @addtogroup Transaction
 * @{
 */

/**
 * @brief TransactionStatus enum class defines current transaction status. All states except Forced are result of
 * verification. Forced status is used when trx is part of synced dag block which is about to be added to DAG
 */
enum class TransactionStatus { Verified = 0, Invalid, LowNonce, InsufficentBalance, Forced };

class DagBlock;
class DagManager;
class FullNode;

/**
 * @brief TransactionManager class verifies and inserts incoming transactions in memory pool and handles saving
 * transactions and all transactions state change
 *
 * Incoming new transactions can be verified with verifyTransaction functions and than inserted in the transaction pool
 * with insertValidatedTransaction. Transactions are kept in transactions memory pool until they are included in a
 * proposed dag block or received in an incoming dag block. Transaction verification consist of:
 * - Verifying the format
 * - Verifying signature
 * - Verifying chan id
 * - Verifying gas
 * - Verifying nonce
 * - Verifying balance
 *
 * Verified transaction inserted in TransactionManager can be in three state:
 * 1. In transactions memory pool
 * 2. In Non-finalized DAG block
 * 3. Finalized transaction
 *
 * Transaction transition to non-finalized block state is done with call to saveTransactionsFromDagBlock.
 * Transaction transition to finalized block state is done with call to updateFinalizedTransactionsStatus
 *
 * Class is thread safe in general with exception of two special methods: updateFinalizedTransactionsStatus and
 * moveNonFinalizedTransactionsToTransactionsPool. See details in function descriptions.
 */
class TransactionManager : public std::enable_shared_from_this<TransactionManager> {
 public:
  TransactionManager(const FullNodeConfig &conf, std::shared_ptr<DbStorage> db, std::shared_ptr<FinalChain> final_chain,
                     addr_t node_addr);

  /**
   * @brief Estimates required gas value to execute transaction
   * @param trx transaction
   * @param proposal_period proposal period
   * @return estimated gas value for transaction
   */
  uint64_t estimateTransactionGas(std::shared_ptr<Transaction> trx, std::optional<PbftPeriod> proposal_period) const;

  /**
   * @brief Gets transactions from pool to include in the block with specified weight limit
   * @param proposal_period proposal period
   * @param weight_limit weight limit
   * @return transactions and weight estimations
   */
  std::pair<SharedTransactions, std::vector<uint64_t>> packTrxs(PbftPeriod proposal_period, uint64_t weight_limit);

  /**
   * @brief Gets all transactions from pool grouped per account
   * @return transactions
   */
  std::vector<SharedTransactions> getAllPoolTrxs();

  /**
   * Saves transactions from dag block which was added to the DAG. Removes transactions from memory pool
   */
  void saveTransactionsFromDagBlock(const SharedTransactions &trxs);

  /**
   * @brief Inserts and verify new transaction to transaction pool
   *
   * @param trx transaction to be processed
   * @return std::pair<bool, std::string> -> pair<OK status, ERR message>
   */
  std::pair<bool, std::string> insertTransaction(const std::shared_ptr<Transaction> &trx);

  /**
   * @brief Invoked when block finalized in final chain
   *
   * @param block_number block number finalized
   */
  void blockFinalized(EthBlockNumber block_number);

  /**
   * @brief Inserts verified transaction to transaction pool
   *
   * @note transaction might be already processed -> they are not processed and inserted again
   * @param tx transaction to be processed
   * @param status transaction status
   * @return true if successfully inserted unseen transactions
   */
  bool insertValidatedTransaction(std::shared_ptr<Transaction> &&tx, const TransactionStatus status);

  /**
   * @param trx_hash transaction hash
   * @return Returns true if tx is known (was successfully verified and pushed into the tx pool), oth
   */
  bool isTransactionKnown(const trx_hash_t &trx_hash);

  size_t getTransactionPoolSize() const;

  /**
   * @brief return true if transaction pool is full
   *
   * @param precentage defines precentage of fullness
   * @return true
   * @return false
   */
  bool isTransactionPoolFull(size_t precentage = 100) const;

  /**
   * @brief return true if non proposable transactions are over the limit
   *
   * @return true
   * @return false
   */
  bool nonProposableTransactionsOverTheLimit() const;

  size_t getNonfinalizedTrxSize() const;

  /**
   * @brief Get the Nonfinalized Trx objects from cache
   *
   * @param hashes
   * @return std::vector<std::shared_ptr<Transaction>>
   */
  std::vector<std::shared_ptr<Transaction>> getNonfinalizedTrx(const std::vector<trx_hash_t> &hashes);

  /**
   * @brief Exclude Finalized transactions
   *
   * @param hashes
   * @return Only transactions that are not finalized
   */
  std::unordered_set<trx_hash_t> excludeFinalizedTransactions(const std::vector<trx_hash_t> &hashes);

  /**
   * @brief Get the block transactions
   *
   * @param blk
   * @return transactions retrieved from pool/db
   */
  std::optional<SharedTransactions> getBlockTransactions(const DagBlock &blk);

  /**
   * @brief Get the  transactions
   *
   * @param blk
   * @return transactions retrieved from pool/db
   */
  std::optional<SharedTransactions> getTransactions(const vec_trx_t &trxs_hashes);

  /**
   * @brief Updates the status of transactions to finalized
   * IMPORTANT: This method is invoked on finalizing a pbft block, it needs to be protected with transactions_mutex_ but
   * the mutex is locked from pbft manager for the entire pbft finalization process to make the finalization atomic
   *
   * @param period_data period data
   * @return number of dag blocks finalized
   */
  void updateFinalizedTransactionsStatus(const PeriodData &period_data);

  /**
   * @brief Moves non-finalized transactions from discarded old dag blocks back to transactions pool
   * IMPORTANT: This method is invoked on finalizing a pbft block, it needs to be protected with transactions_mutex_ but
   * the mutex is locked from pbft manager for the entire pbft finalization process to make the finalization atomic
   *
   * @param transactions transactions to move
   */
  void moveNonFinalizedTransactionsToTransactionsPool(std::unordered_set<trx_hash_t> &&transactions);

  /**
   * @brief Retrieves transactions mutex, only to be used when finalizing pbft block
   *
   * @return mutex
   */
  std::shared_mutex &getTransactionsMutex() { return transactions_mutex_; }

  /**
   * @brief Gets transactions from transactions pool
   *
   * @param trx_to_query
   *
   * @return Returns transactions found and list of missing transactions hashes
   */
  std::pair<std::vector<std::shared_ptr<Transaction>>, std::vector<trx_hash_t>> getPoolTransactions(
      const std::vector<trx_hash_t> &trx_to_query) const;

  /**
   * @brief Have transactions been recently dropped due to queue reaching max size
   * This call is thread-safe
   * @return Returns true if txs were dropped
   */
  bool transactionsDropped() const;

  std::shared_ptr<Transaction> getTransaction(const trx_hash_t &hash) const;
  std::shared_ptr<Transaction> getNonFinalizedTransaction(const trx_hash_t &hash) const;
  unsigned long getTransactionCount() const;
  void recoverNonfinalizedTransactions();
  std::pair<TransactionStatus, std::string> verifyTransaction(const std::shared_ptr<Transaction> &trx) const;

 private:
  addr_t getFullNodeAddress() const;

 public:
  util::Event<TransactionManager, h256> const transaction_accepted_{};

 private:
  const FullNodeConfig kConf;
  // Guards updating transaction status
  // Transactions can be in one of three states:
  // 1. In transactions pool; 2. In non-finalized Dag block 3. Executed
  mutable std::shared_mutex transactions_mutex_;
  TransactionQueue transactions_pool_;
  std::unordered_map<trx_hash_t, std::shared_ptr<Transaction>> nonfinalized_transactions_in_dag_;
  std::unordered_map<trx_hash_t, std::shared_ptr<Transaction>> recently_finalized_transactions_;
  uint64_t trx_count_ = 0;

  const uint64_t kDagBlockGasLimit;
  const uint64_t kEstimateGasLimit = 200000;
  const uint64_t kRecentlyFinalizedTransactionsMax = 50000;

  std::shared_ptr<DbStorage> db_{nullptr};
  std::shared_ptr<FinalChain> final_chain_{nullptr};

  LOG_OBJECTS_DEFINE
};

/** @}*/

}  // namespace taraxa
