#pragma once

#include "config/config.hpp"
#include "transaction/transaction.hpp"

namespace taraxa {

using std::string;
class DagBlock;
class FullNode;
class DbStorage;

class TransactionQueue {
 public:
  enum class VerifyMode : uint8_t { normal, skip_verify_sig };
  TransactionQueue(addr_t node_addr) { LOG_OBJECTS_CREATE("TRXQU"); }
  ~TransactionQueue() { stop(); }

  void start();
  void stop();

  /**
   * @brief Inserts transaction into tx queue and database
   *
   * @param trx
   * @param queue_verified flag if tx should be inserted into the verified or unverified queue
   * @param tx_verified flag if tx was actually verified (verification is skipped in tests. TODO: bad architecture)
   * @return true if trx was inserted, otherwise false(might be already added by different thread)
   */
  bool insert(Transaction const &trx, bool queue_verified, bool tx_verified, const std::shared_ptr<DbStorage> &db);

  /**
   * @brief Inserts batch of unverified transactions into the queue and database at once
   * @note Some of the provided txs might not be inserted as they were already inserted by different thread
   *
   * @param trxs
   * @param verify
   * @param db
   */
  void insertUnverifiedTrxs(const std::vector<Transaction> &trxs, const std::shared_ptr<DbStorage> &db);

  Transaction top();
  void pop();
  std::pair<trx_hash_t, std::shared_ptr<Transaction>> getUnverifiedTransaction();
  void removeTransactionFromBuffer(trx_hash_t const &hash);
  void addTransactionToVerifiedQueue(trx_hash_t const &hash, std::shared_ptr<Transaction> trx);
  std::unordered_map<trx_hash_t, Transaction> moveVerifiedTrxSnapShot(uint16_t max_trx_to_pack = 0);
  std::unordered_map<trx_hash_t, Transaction> getVerifiedTrxSnapShot() const;
  std::pair<size_t, size_t> getTransactionQueueSize() const;
  size_t getTransactionBufferSize() const;
  std::vector<Transaction> getNewVerifiedTrxSnapShot();
  void removeBlockTransactionsFromQueue(vec_trx_t const &all_block_trxs);
  unsigned long getVerifiedTrxCount() const;
  std::shared_ptr<Transaction> getTransaction(trx_hash_t const &hash) const;

 private:
  using uLock = boost::unique_lock<boost::shared_mutex>;
  using sharedLock = boost::shared_lock<boost::shared_mutex>;
  addr_t getFullNodeAddress() const;
  std::atomic<bool> stopped_ = true;
  bool new_verified_transactions_ = true;

  std::unordered_map<trx_hash_t, std::shared_ptr<Transaction>> queued_trxs_;  // all trx
  std::unordered_map<trx_hash_t, std::shared_ptr<Transaction>> verified_trxs_;
  mutable boost::shared_mutex main_shared_mutex_;
  std::deque<std::pair<trx_hash_t, std::shared_ptr<Transaction>>> unverified_hash_qu_;
  mutable boost::shared_mutex shared_mutex_for_unverified_qu_;
  boost::condition_variable_any cond_for_unverified_qu_;

  LOG_OBJECTS_DEFINE
};

}  // namespace taraxa