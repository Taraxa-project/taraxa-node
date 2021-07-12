#pragma once

#include "config/config.hpp"
#include "transaction.hpp"

namespace taraxa {

using std::string;
class DagBlock;
class FullNode;

class TransactionQueue {
 public:
  enum class VerifyMode : uint8_t { normal, skip_verify_sig };
  TransactionQueue(addr_t node_addr) { LOG_OBJECTS_CREATE("TRXQU"); }
  ~TransactionQueue() { stop(); }

  void start();
  void stop();
  void insert(Transaction const &trx, bool verify);

  /**
   * @brief Insert batch of unverified transactions at once
   * @param trxs
   * @param verify
   */
  void insertUnverifiedTrxs(const vector<Transaction> &trxs);

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
  using upgradableLock = boost::upgrade_lock<boost::shared_mutex>;
  using upgradeLock = boost::upgrade_to_unique_lock<boost::shared_mutex>;
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