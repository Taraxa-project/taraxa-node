#ifndef TARAXA_NODE_TRANSACTION_MANAGER_HPP
#define TARAXA_NODE_TRANSACTION_MANAGER_HPP

#include "config.hpp"
#include "transaction.hpp"
#include "transaction_queue.hpp"
#include "util/simple_event.hpp"

namespace taraxa {

using std::string;
class DagBlock;
class FullNode;

using TransactionRLPTable = ExpirationCacheMap<trx_hash_t, taraxa::bytes>;
using AccountNonceTable = StatusTable<addr_t, val_t>;

/**
 * Manage transactions within an epoch
 * 1. Check existence of transaction (seen in queue? in db?)
 * 2. Push to transaction queue and verify
 *
 */

class TransactionManager
    : public std::enable_shared_from_this<TransactionManager> {
 public:
  util::SimpleEvent<trx_hash_t> const event_transaction_accepted{};

  using uLock = std::unique_lock<std::mutex>;
  enum class VerifyMode : uint8_t { normal, skip_verify_sig };

  explicit TransactionManager(TestParamsConfig const &conf)
      : conf_(conf), accs_nonce_() {}
  explicit TransactionManager(std::shared_ptr<DbStorage> db)
      : db_(db), accs_nonce_(), conf_() {}
  std::shared_ptr<TransactionManager> getShared() {
    try {
      return shared_from_this();
    } catch (std::bad_weak_ptr &e) {
      LOG(log_er_) << e.what() << std::endl;
      return nullptr;
    }
  }

  virtual ~TransactionManager() { stop(); }

  void start();
  void stop();
  void setFullNode(std::shared_ptr<FullNode> full_node);
  std::pair<bool, std::string> insertTrx(Transaction const &trx,
                                         taraxa::bytes const &trx_serialized,
                                         bool verify);

  /**
   * The following function will require a lock for verified qu
   */
  void packTrxs(vec_trx_t &to_be_packed_trx, DagFrontier &frontier,
                uint16_t max_trx_to_pack = 0);
  void setVerifyMode(VerifyMode mode) { mode_ = mode; }

  std::pair<bool, std::string> verifyTransaction(Transaction const &trx) const;

  std::unordered_map<trx_hash_t, Transaction> getVerifiedTrxSnapShot() const;
  std::vector<taraxa::bytes> getNewVerifiedTrxSnapShotSerialized();
  std::pair<size_t, size_t> getTransactionQueueSize() const;

  // Verify transactions in broadcasted blocks
  bool verifyBlockTransactions(DagBlock const &blk,
                               std::vector<Transaction> const &trxs);

  std::shared_ptr<std::pair<Transaction, taraxa::bytes>> getTransaction(
      trx_hash_t const &hash) const;
  unsigned long getTransactionCount() const;
  // Received block means these trxs are packed by others

  bool saveBlockTransactionAndDeduplicate(
      DagBlock const &blk, std::vector<Transaction> const &some_trxs);

  void updateNonce(DagBlock const &blk, DagFrontier const &frontier);

  TransactionQueue &getTransactionQueue() { return trx_qu_; }

 private:
  DagFrontier getDagFrontier();
  void setDagFrontier(DagFrontier const &frontier);
  void verifyQueuedTrxs();
  size_t num_verifiers_ = 4;
  addr_t getFullNodeAddress() const;
  VerifyMode mode_ = VerifyMode::normal;
  std::atomic<bool> stopped_ = true;
  std::weak_ptr<FullNode> full_node_;
  std::shared_ptr<DbStorage> db_ = nullptr;
  AccountNonceTable accs_nonce_;
  TransactionQueue trx_qu_;
  DagFrontier dag_frontier_;  // Dag boundary seen up to now
  std::atomic<unsigned long> trx_count_ = 0;
  TestParamsConfig conf_;
  std::vector<std::thread> verifiers_;

  mutable std::mutex mu_for_nonce_table_;
  mutable std::mutex mu_for_transactions_;
  mutable std::shared_mutex mu_for_dag_frontier_;
  mutable dev::Logger log_si_{
      dev::createLogger(dev::Verbosity::VerbositySilent, "TRXMGR")};
  mutable dev::Logger log_er_{
      dev::createLogger(dev::Verbosity::VerbosityError, "TRXMGR")};
  mutable dev::Logger log_wr_{
      dev::createLogger(dev::Verbosity::VerbosityWarning, "TRXMGR")};
  mutable dev::Logger log_nf_{
      dev::createLogger(dev::Verbosity::VerbosityInfo, "TRXMGR")};
  mutable dev::Logger log_dg_{
      dev::createLogger(dev::Verbosity::VerbosityDebug, "TRXMGR")};
};
}  // namespace taraxa

#endif