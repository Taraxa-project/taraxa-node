#ifndef TARAXA_NODE_TRANSACTION_MANAGER_HPP
#define TARAXA_NODE_TRANSACTION_MANAGER_HPP

#include "aleth/filter_api.hpp"
#include "aleth/pending_block.hpp"
#include "config.hpp"
#include "transaction.hpp"
#include "transaction_queue.hpp"
#include "util/simple_event.hpp"
#include "transaction_status.hpp"

namespace taraxa {

using std::string;
class DagBlock;
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

class TransactionManager
    : public std::enable_shared_from_this<TransactionManager> {
 public:
  util::SimpleEvent<trx_hash_t> const event_transaction_accepted{};

  using uLock = std::unique_lock<std::mutex>;
  enum class VerifyMode : uint8_t { normal, skip_verify_sig };

  TransactionManager(FullNodeConfig const &conf, addr_t node_addr,
                     std::shared_ptr<DbStorage> db, boost::log::sources::severity_channel_logger<> log_time);
  explicit TransactionManager(std::shared_ptr<DbStorage> db, addr_t node_addr)
      : db_(db),
        conf_(),
        trx_qu_(node_addr),
        node_addr_(node_addr) {
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
  void setNetwork(std::shared_ptr<Network> network);
  void setWsServer(std::shared_ptr<net::WSServer> ws_server);
  std::pair<bool, std::string> insertTrx(Transaction const &trx, bool verify);

  void setPendingBlock(std::shared_ptr<aleth::PendingBlock> pending_block) {
    pending_block_ = pending_block;
    filter_api_ = aleth::NewFilterAPI();
    event_transaction_accepted.sub([=](auto const &h) {
      pending_block_->add_transactions(vector{h});
      filter_api_->note_pending_transactions(vector{h});
    });
  }
  auto getPendingBlock() const { return pending_block_; }
  auto getFilterAPI() const { return filter_api_; }

  /**
   * The following function will require a lock for verified qu
   */
  void packTrxs(vec_trx_t &to_be_packed_trx, DagFrontier &frontier,
                uint16_t max_trx_to_pack = 0);
  void setVerifyMode(VerifyMode mode) { mode_ = mode; }

  // Insert new transaction to unverified queue or if verify flag true
  // synchronously verify and insert into verified queue
  std::pair<bool, std::string> insertTransaction(Transaction const &trx,
                                                 bool verify = false);
  // Transactions coming from broadcasting is less critical
  uint32_t insertBroadcastedTransactions(
      std::vector<taraxa::bytes> const &transactions);

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

  TransactionQueue &getTransactionQueue() { return trx_qu_; }
  void setDagFrontier(DagFrontier const &frontier);

 private:
  DagFrontier getDagFrontier();
  void verifyQueuedTrxs();
  size_t num_verifiers_ = 4;
  addr_t getFullNodeAddress() const;
  VerifyMode mode_ = VerifyMode::normal;
  std::atomic<bool> stopped_ = true;
  std::shared_ptr<DbStorage> db_ = nullptr;
  TransactionQueue trx_qu_;
  DagFrontier dag_frontier_;  // Dag boundary seen up to now
  std::atomic<unsigned long> trx_count_ = 0;
  FullNodeConfig conf_;
  std::vector<std::thread> verifiers_;
  std::shared_ptr<Network> network_;
  std::shared_ptr<net::WSServer> ws_server_;
  addr_t node_addr_;
  std::shared_ptr<DagManager> dag_mgr_;
  boost::log::sources::severity_channel_logger<> log_time_;
  std::shared_ptr<aleth::PendingBlock> pending_block_;
  std::shared_ptr<aleth::FilterAPI> filter_api_;

  mutable std::mutex mu_for_nonce_table_;
  mutable std::mutex mu_for_transactions_;
  mutable std::shared_mutex mu_for_dag_frontier_;
  LOG_OBJECTS_DEFINE;
};
}  // namespace taraxa

#endif