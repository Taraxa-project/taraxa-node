#pragma once

#include "config/config.hpp"
#include "logger/log.hpp"
#include "transaction.hpp"
#include "transaction_queue.hpp"
#include "transaction_status.hpp"
#include "util/event.hpp"

namespace taraxa {

using std::string;
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
  using uLock = std::unique_lock<std::mutex>;
  enum class VerifyMode : uint8_t { normal, skip_verify_sig };

  util::Event<TransactionManager, h256> const transaction_accepted_{};

  TransactionManager(FullNodeConfig const &conf, addr_t node_addr, std::shared_ptr<DbStorage> db,
                     logger::Logger log_time);
  explicit TransactionManager(std::shared_ptr<DbStorage> db, addr_t node_addr)
      : db_(db), conf_(), trx_qu_(node_addr), node_addr_(node_addr) {
    LOG_OBJECTS_CREATE("TRXMGR");
  }
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
  void setNetwork(std::weak_ptr<Network> network);

  /**
   * The following function will require a lock for verified qu
   */
  void packTrxs(vec_trx_t &to_be_packed_trx, uint16_t max_trx_to_pack = 0);
  void setVerifyMode(VerifyMode mode) { mode_ = mode; }

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
   * @param raw_trxs transactions to be processed
   * @return number of successfully inserted unseen transactions
   */
  uint32_t insertBroadcastedTransactions(const std::vector<taraxa::bytes> &raw_trxs);

  std::pair<bool, std::string> verifyTransaction(Transaction const &trx) const;

  std::unordered_map<trx_hash_t, Transaction> getVerifiedTrxSnapShot() const;
  std::vector<taraxa::bytes> getNewVerifiedTrxSnapShotSerialized();
  std::pair<size_t, size_t> getTransactionQueueSize() const;
  size_t getTransactionBufferSize() const;

  // Verify transactions in broadcasted blocks
  bool verifyBlockTransactions(DagBlock const &blk, std::vector<Transaction> const &trxs);

  std::shared_ptr<std::pair<Transaction, taraxa::bytes>> getTransaction(trx_hash_t const &hash) const;
  unsigned long getTransactionCount() const;
  void addTrxCount(unsigned long num);
  // Received block means these trxs are packed by others

  TransactionQueue &getTransactionQueue() { return trx_qu_; }

 private:
  /**
   * @brief Checks if transaction queue is overflowed
   *
   * @return true if transaction queue is overflowed, otherwise false
   */
  bool checkQueueOverflow();

 private:
  void verifyQueuedTrxs();
  size_t num_verifiers_ = 4;
  addr_t getFullNodeAddress() const;
  VerifyMode mode_ = VerifyMode::normal;
  std::atomic<bool> stopped_ = true;
  std::shared_ptr<DbStorage> db_ = nullptr;
  FullNodeConfig conf_;
  TransactionQueue trx_qu_;
  std::atomic<unsigned long> trx_count_ = 0;
  std::vector<std::thread> verifiers_;
  std::weak_ptr<Network> network_;
  std::shared_ptr<net::WSServer> ws_server_;
  addr_t node_addr_;
  std::shared_ptr<DagManager> dag_mgr_;
  logger::Logger log_time_;

  LOG_OBJECTS_DEFINE
};
}  // namespace taraxa
