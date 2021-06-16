#pragma once

#include "config/config.hpp"
#include "logger/log.hpp"
#include "transaction_manager/transaction.hpp"
#include "transaction_manager/transaction_queue.hpp"
#include "transaction_manager/transaction_status.hpp"
#include "util/event.hpp"

namespace taraxa {

class DagBlock;
class DagManager;
class FullNode;
class Network;

namespace net {
class WSServer;
}

/**
 * Manage transactions within an epoch
 * 1. Check existence of transaction (seen in queue? in db?)
 * 2. Push to transaction queue and verify
 *
 */
class TransactionManager : public std::enable_shared_from_this<TransactionManager> {
 public:
  enum class VerifyMode : uint8_t { normal, skip_verify_sig };

  TransactionManager(FullNodeConfig const &conf, addr_t node_addr, std::shared_ptr<DbStorage> db,
                     VerifyMode mode = VerifyMode::normal);
  virtual ~TransactionManager();

  void start();
  void stop();

  /**
   * @brief Sets network
   * @note: Not thread-safe !
   *
   * @param network
   */
  void setNetwork(std::weak_ptr<Network> network);

  /**
   * The following function will require a lock for verified qu
   */
  void packTrxs(vec_trx_t &to_be_packed_trx, uint16_t max_trx_to_pack = 0);

  /**
   * @brief Inserts new transaction to queue and db
   *
   * @param trx transaction to be processed
   * @param verify - if set to true, tx is also verified and inserted (if valid) into verified queue and db
   *                 otherwise inserted into unverified queue and db
   * @param broadcast - if set to true, tx is broadcasted to the network
   * @return std::pair<bool, std::string> -> pair<OK status, ERR message>
   */
  std::pair<bool, std::string> insertTransaction(Transaction const &trx, bool verify = true, bool broadcast = true);

  /**
   * @brief Inserts batch of unverified broadcasted transactions to unverified queue and db
   *
   * @note Some of the transactions might be already processed -> they are not processed and inserted again
   * @param txs transactions to be processed
   * @return number of successfully inserted unseen transactions
   */
  uint32_t insertBroadcastedTransactions(const std::vector<Transaction> &txs);

  size_t getVerifiedTrxCount() const;
  std::vector<Transaction> getNewVerifiedTrxSnapShot();
  std::pair<size_t, size_t> getTransactionQueueSize() const;
  size_t getTransactionBufferSize() const;

  // Verify transactions in broadcasted blocks
  bool verifyBlockTransactions(DagBlock const &blk, std::vector<Transaction> const &trxs);

  // Update the status of transactions to finalized and remove from transactions column
  void updateFinalizedTransactionsStatus(SyncBlock const &sync_block);

  std::shared_ptr<std::pair<Transaction, taraxa::bytes>> getTransaction(trx_hash_t const &hash) const;
  unsigned long getTransactionCount() const;
  void addTrxCount(unsigned long num);
  // Received block means these trxs are packed by others

  // TODO: not a good idea to return reference to the private tx_queue
  TransactionQueue &getTransactionQueue();

 private:
  /**
   * @brief Checks if transaction queue is overflowed
   *
   * @return true if transaction queue is overflowed, otherwise false
   */
  bool checkQueueOverflow();

  addr_t getFullNodeAddress() const;
  void verifyQueuedTrxs();
  std::pair<bool, std::string> verifyTransaction(Transaction const &trx) const;

 public:
  util::Event<TransactionManager, h256> const transaction_accepted_{};

 private:
  const size_t num_verifiers_{4};
  const VerifyMode mode_;
  const FullNodeConfig conf_;

  std::atomic<bool> stopped_{true};
  std::atomic<unsigned long> trx_count_{0};

  TransactionQueue trx_qu_;
  std::vector<std::thread> verifiers_;
  ExpirationCache<trx_hash_t> seen_txs_;

  std::shared_ptr<DbStorage> db_{nullptr};
  std::weak_ptr<Network> network_;
  std::shared_ptr<DagManager> dag_mgr_{nullptr};

  // Guards updating transaction status based on retrieved status
  std::shared_mutex transaction_status_mutex_;

  LOG_OBJECTS_DEFINE
};
}  // namespace taraxa
