#ifndef TARAXA_NODE_TRANSACTION_QUEUE_HPP
#define TARAXA_NODE_TRANSACTION_QUEUE_HPP

#include "config.hpp"
#include "transaction.hpp"

namespace taraxa {

using std::string;
class DagBlock;
class FullNode;

class TransactionQueue {
 public:
  enum class VerifyMode : uint8_t { normal, skip_verify_sig };
  using listIter = std::list<Transaction>::iterator;
  TransactionQueue(addr_t node_addr) { LOG_OBJECTS_CREATE("TRXQU"); }
  ~TransactionQueue() { stop(); }

  void start();
  void stop();
  void insert(Transaction const &trx, bool verify);
  Transaction top();
  void pop();
  std::pair<trx_hash_t, listIter> getUnverifiedTransaction();
  void removeTransactionFromBuffer(trx_hash_t const &hash);
  void addTransactionToVerifiedQueue(trx_hash_t const &hash,
                                     std::list<Transaction>::iterator);
  std::unordered_map<trx_hash_t, Transaction> moveVerifiedTrxSnapShot(
      uint16_t max_trx_to_pack = 0);
  std::unordered_map<trx_hash_t, Transaction> getVerifiedTrxSnapShot() const;
  std::pair<size_t, size_t> getTransactionQueueSize() const;
  std::vector<Transaction> getNewVerifiedTrxSnapShot();
  std::unordered_map<trx_hash_t, Transaction> removeBlockTransactionsFromQueue(
      vec_trx_t const &all_block_trxs);
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

  std::list<Transaction> trx_buffer_;
  std::unordered_map<trx_hash_t, listIter> queued_trxs_;  // all trx
  mutable boost::shared_mutex shared_mutex_for_queued_trxs_;

  std::unordered_map<trx_hash_t, listIter> verified_trxs_;
  mutable boost::shared_mutex shared_mutex_for_verified_qu_;
  std::deque<std::pair<trx_hash_t, listIter>> unverified_hash_qu_;
  mutable boost::shared_mutex shared_mutex_for_unverified_qu_;
  boost::condition_variable_any cond_for_unverified_qu_;

  LOG_OBJECTS_DEFINE;
};

}  // namespace taraxa

#endif