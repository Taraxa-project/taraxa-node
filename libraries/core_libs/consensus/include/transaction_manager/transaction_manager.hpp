#pragma once

#include "common/event.hpp"
#include "config/config.hpp"
#include "logger/logger.hpp"
#include "storage/storage.hpp"
#include "transaction/transaction.hpp"

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
  enum class VerifyMode : uint8_t { normal, skip_verify_sig };

  TransactionManager(FullNodeConfig const &conf, addr_t node_addr, std::shared_ptr<DbStorage> db,
                     VerifyMode mode = VerifyMode::normal);

  /**
   * Retrieves transactions to be included in a proposed pbft block
   */
  SharedTransactions packTrxs(uint16_t max_trx_to_pack = 0);

  /**
   * Saves transactions from dag block which was added to the DAG. Removes transactions from memory pool
   */
  void saveTransactionsFromDagBlock(SharedTransactions const &trxs);

  /**
   * @brief Inserts new transaction to transaction pool
   *
   * @param trx transaction to be processed
   * @return std::pair<bool, std::string> -> pair<OK status, ERR message>
   */
  std::pair<bool, std::string> insertTransaction(Transaction const &trx);

  /**
   * @brief Inserts batch of unverified broadcasted transactions to transaction pool
   *
   * @note Some of the transactions might be already processed -> they are not processed and inserted again
   * @param txs transactions to be processed
   * @return number of successfully inserted unseen transactions
   */
  uint32_t insertBroadcastedTransactions(const SharedTransactions &txs);

  /**
   * Returns a copy of transactions pool
   */
  SharedTransactions getTransactionsSnapShot() const;

  size_t getTransactionPoolSize() const;

  size_t getNonfinalizedTrxSize() const;

  // Check transactions are present in broadcasted blocks
  bool checkBlockTransactions(DagBlock const &blk);

  // Update the status of transactions to finalized and remove from transactions column
  void updateFinalizedTransactionsStatus(SyncBlock const &sync_block);

  std::shared_ptr<Transaction> getTransaction(trx_hash_t const &hash) const;
  unsigned long getTransactionCount() const;
  void recoverNonfinalizedTransactions();

 private:
  /**
   * @brief Checks if transaction pool is overflowed
   *
   * @return true if transaction pool is overflowed, otherwise false
   */
  bool checkMemoryPoolOverflow();

  addr_t getFullNodeAddress() const;
  std::pair<bool, std::string> verifyTransaction(Transaction const &trx) const;

 public:
  util::Event<TransactionManager, h256> const transaction_accepted_{};

 private:
  const VerifyMode mode_;
  const FullNodeConfig conf_;

  std::atomic_uint64_t trx_count_ = 0;

  ThreadSafeMap<trx_hash_t, std::shared_ptr<Transaction>> transactions_pool_;
  ThreadSafeSet<trx_hash_t> nonfinalized_transactions_in_dag_;
  ExpirationCache<trx_hash_t> seen_txs_;
  mutable bool transactions_pool_changed_ = true;

  std::shared_ptr<DbStorage> db_{nullptr};

  // Guards updating transaction status
  // Transactions can be in one of three states:
  // 1. In transactions pool; 2. In non-finalized Dag block 3. Executed
  mutable std::shared_mutex transactions_mutex_;

  LOG_OBJECTS_DEFINE
};
}  // namespace taraxa
