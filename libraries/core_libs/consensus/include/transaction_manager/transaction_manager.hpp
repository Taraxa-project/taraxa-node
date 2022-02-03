#pragma once

#include "common/event.hpp"
#include "config/config.hpp"
#include "final_chain/final_chain.hpp"
#include "logger/logger.hpp"
#include "storage/storage.hpp"
#include "transaction/transaction.hpp"
#include "transaction_queue.hpp"

namespace taraxa {

class DagBlock;
class DagManager;
class FullNode;

/**
 * Manage transactions within an epoch
 * 1. Check existence of transaction
 * 2. Push to transaction pool and verify
 *
 */
class TransactionManager : public std::enable_shared_from_this<TransactionManager> {
 public:
  TransactionManager(FullNodeConfig const &conf, std::shared_ptr<DbStorage> db, std::shared_ptr<FinalChain> final_chain,
                     addr_t node_addr);

  /**
   * Retrieves transactions to be included in a proposed pbft block
   */
  SharedTransactions packTrxs(uint16_t max_trx_to_pack = 0);

  /**
   * Saves transactions from dag block which was added to the DAG. Removes transactions from memory pool
   */
  void saveTransactionsFromDagBlock(SharedTransactions const &trxs);

  /**
   * @brief Inserts and verify new transaction to transaction pool
   *
   * @param trx transaction to be processed
   * @return std::pair<bool, std::string> -> pair<OK status, ERR message>
   */
  std::pair<bool, std::string> insertTransaction(Transaction const &trx);

  /**
   * @brief Inserts batch of verified transactions to transaction pool
   *
   * @note Some of the transactions might be already processed -> they are not processed and inserted again
   * @param txs transactions to be processed
   * @return number of successfully inserted unseen transactions
   */
  uint32_t insertValidatedTransactions(const SharedTransactions &txs);

  /**
   * @brief Marks transaction as seen and returns if was seen before
   *
   * @param trx_hash transaction hash
   * @return true if seen
   */
  bool markTransactionSeen(const trx_hash_t &trx_hash);

  size_t getTransactionPoolSize() const;

  size_t getNonfinalizedTrxSize() const;

  /**
   * @brief Get the Nonfinalized Trx objects from cache
   *
   * @param hashes
   * @param sorted
   * @return std::vector<std::shared_ptr<Transaction>>
   */
  std::vector<std::shared_ptr<Transaction>> getNonfinalizedTrx(const std::vector<trx_hash_t> &hashes,
                                                               bool sorted = false);

  // Check transactions are present in broadcasted blocks
  bool checkBlockTransactions(DagBlock const &blk);

  /**
   * @brief Updates the status of transactions to finalized
   * IMPORTANT: This method is invoked on finalizing a pbft block, it needs to be protected with transactions_mutex_ but
   * the mutex is locked from pbft manager for the entire pbft finalization process to make the finalization atomic
   *
   * @param anchor Anchor of the finalized pbft block
   * @param period Period finalized
   * @param dag_order Dag order of the finalized pbft block
   *
   * @return number of dag blocks finalized
   */
  void updateFinalizedTransactionsStatus(SyncBlock const &sync_block);

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

  std::shared_ptr<Transaction> getTransaction(trx_hash_t const &hash) const;
  std::shared_ptr<Transaction> getNonFinalizedTransaction(trx_hash_t const &hash) const;
  unsigned long getTransactionCount() const;
  void recoverNonfinalizedTransactions();
  std::pair<bool, std::string> verifyTransaction(const std::shared_ptr<Transaction> &trx) const;

 private:
  /**
   * @brief Checks if transaction pool is overflowed
   *
   * @return true if transaction pool is overflowed, otherwise false
   */
  bool checkMemoryPoolOverflow();

  addr_t getFullNodeAddress() const;

 public:
  util::Event<TransactionManager, h256> const transaction_accepted_{};

 private:
  const FullNodeConfig conf_;
  // Guards updating transaction status
  // Transactions can be in one of three states:
  // 1. In transactions pool; 2. In non-finalized Dag block 3. Executed
  mutable std::shared_mutex transactions_mutex_;
  TransactionQueue transactions_pool_;
  std::unordered_map<trx_hash_t, std::shared_ptr<Transaction>> nonfinalized_transactions_in_dag_;
  uint64_t trx_count_ = 0;

  ExpirationCache<trx_hash_t> seen_txs_;

  std::shared_ptr<DbStorage> db_{nullptr};
  std::shared_ptr<FinalChain> final_chain_{nullptr};

  LOG_OBJECTS_DEFINE
};
}  // namespace taraxa
