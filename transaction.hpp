/*
 * @Copyright: Taraxa.io
 * @Author: Chia-Chun Lin
 * @Date: 2019-02-27 12:27:18
 * @Last Modified by: Chia-Chun Lin
 * @Last Modified time: 2019-05-15 15:52:30
 */

#ifndef TRANSACTION_HPP
#define TRANSACTION_HPP
#include <libdevcore/RLP.h>
#include <libdevcore/SHA3.h>
#include <libdevcrypto/Common.h>
#include <libethcore/Common.h>
#include <condition_variable>
#include <iostream>
#include <list>
#include <queue>
#include <thread>
#include <vector>
#include "SimpleDBFace.h"
#include "dag_block.hpp"
#include "libdevcore/Log.h"
#include "proto/taraxa_grpc.grpc.pb.h"
#include "util.hpp"

namespace taraxa {

using std::string;
class DagBlock;

enum class TransactionStatus {
  invalid,
  in_block,  // confirmed state, inside of block created by us or someone else
  in_queue,  // not packed yet
  unseen
};
/**
 * simple thread_safe hash
 * keep track of transaction state
 */
using TransactionStatusTable = StatusTable<trx_hash_t, TransactionStatus>;

/**
 * Note:
 * Need to sign first then sender() and hash() is available
 */

class Transaction {
 public:
  enum class Type : uint8_t { Null, Create, Call };
  Transaction() = default;
  Transaction(::taraxa_grpc::ProtoTransaction const &t)
      : hash_(t.hash()),
        type_(toEnum<Type>(t.type())),
        nonce_(stoull(t.nonce())),
        value_(stoull(t.value())),
        gas_price_(t.gas_price()),
        gas_(t.gas()),
        receiver_(t.receiver()),
        sig_(t.sig()),
        data_(str2bytes(t.data())) {}
  // default constructor
  Transaction(trx_hash_t const &hash, Type type, bal_t const &nonce,
              bal_t const &value, val_t const &gas_price, val_t const &gas,
              addr_t const &receiver, sig_t const &sig, bytes const &data) try
      : hash_(hash),
        type_(type),
        nonce_(nonce),
        value_(value),
        gas_price_(gas_price),
        gas_(gas),
        receiver_(receiver),
        sig_(sig),
        data_(data) {
  } catch (std::exception &e) {
    std::cerr << e.what() << std::endl;
  }

  // sign message call
  Transaction(bal_t const &nonce, bal_t const &value, val_t const &gas_price,
              val_t const &gas, addr_t const &receiver, bytes const &data,
              secret_t const &sk) try : type_(Transaction::Type::Call),
                                        nonce_(nonce),
                                        value_(value),
                                        gas_price_(gas_price),
                                        gas_(gas),
                                        receiver_(receiver),
                                        data_(data) {
    sign(sk);
    updateHash();
  } catch (std::exception &e) {
    std::cerr << e.what() << std::endl;
  }

  // sign contract create
  Transaction(bal_t const &nonce, bal_t const &value, val_t const &gas_price,
              val_t const &gas, bytes const &data, secret_t const &sk) try
      : type_(Transaction::Type::Create),
        nonce_(nonce),
        value_(value),
        gas_price_(gas_price),
        gas_(gas),
        data_(data) {
    sign(sk);
    updateHash();
  } catch (std::exception &e) {
    std::cerr << e.what() << std::endl;
  }
  Transaction(Transaction &&trx) = default;
  Transaction(Transaction const &trx) = default;
  Transaction(stream &strm);
  Transaction(string const &json);
  Transaction(dev::RLP const &_r);
  void serializeRLP(dev::RLPStream &s);
  trx_hash_t getHash() const { return hash_; }
  Type getType() const { return type_; }
  bal_t getNonce() const { return nonce_; }
  bal_t getValue() const { return value_; }
  val_t getGasPrice() const { return gas_price_; }
  val_t getGas() const { return gas_; }
  addr_t getSender() const { return sender(); }
  addr_t getReceiver() const { return receiver_; }
  sig_t getSig() const { return sig_; }
  bytes getData() const { return data_; }

  friend std::ostream &operator<<(std::ostream &strm,
                                  Transaction const &trans) {
    strm << "[Transaction] " << std::endl;
    strm << "  hash: " << trans.hash_ << std::endl;
    strm << "  type: " << asInteger(trans.type_) << std::endl;
    strm << "  nonce: " << trans.nonce_ << std::endl;
    strm << "  value: " << trans.value_ << std::endl;
    strm << "  gas_price: " << trans.gas_price_ << std::endl;
    strm << "  gas: " << trans.gas_ << std::endl;
    strm << "  sig: " << trans.sig_ << std::endl;
    strm << "  receiver: " << trans.receiver_ << std::endl;
    strm << "  data: " << bytes2str(trans.data_) << std::endl;
    return strm;
  }
  bool serialize(stream &strm) const;
  bool deserialize(stream &strm);
  string getJsonStr() const;
  bool operator==(Transaction const &other) const {
    return this->sha3(true) == other.sha3(true);
  }

  Transaction &operator=(Transaction &&other) = default;
  Transaction &operator=(Transaction const &other) = default;
  bool operator<(Transaction const &other) const { return hash_ < other.hash_; }
  void sign(secret_t const &sk);
  // @returns sender of the transaction from the signature (and hash).
  addr_t sender() const;
  void updateHash() {
    if (!hash_) {
      hash_ = dev::sha3(rlp(true));
    }
  }
  bool verifySig() const;

 protected:
  // Serialises this transaction to an RLPStream.
  void streamRLP(dev::RLPStream &s, bool include_sig) const;
  // @returns the RLP serialisation of this transaction.
  bytes rlp(bool include_sig) const;
  // @returns the SHA3 hash of the RLP serialisation of this transaction.
  blk_hash_t sha3(bool include_sig) const;
  trx_hash_t hash_;
  Type type_ = Type::Null;
  bal_t nonce_ = 0;
  bal_t value_ = 0;
  val_t gas_price_;
  val_t gas_;
  addr_t receiver_;
  sig_t sig_;
  bytes data_;

  mutable addr_t cached_sender_;  ///< Cached sender, determined from signature.
};

/**
 * Thread safe
 * Firstly import unverified transaction to deque
 * Verifier threads will verify and move the transaction to verified priority
 */

class TransactionQueue {
 public:
  struct SizeLimit {
    size_t current = 1024;
    size_t future = 1024;
  };
  enum class VerifyMode : uint8_t { normal, skip_verify_sig };
  TransactionQueue(TransactionStatusTable &status, size_t num_verifiers)
      : trx_status_(status), num_verifiers_(num_verifiers) {}
  TransactionQueue(TransactionStatusTable &status, size_t num_verifiers,
                   unsigned current_capacity, unsigned future_capacity)
      : trx_status_(status),
        num_verifiers_(num_verifiers),
        current_capacity_(current_capacity),
        future_capacity_(future_capacity) {}
  ~TransactionQueue() {
    if (!stopped_) {
      stop();
    }
  }
  void start();
  void stop();
  bool insert(Transaction trx, bool critical);
  Transaction top();
  void pop();
  std::unordered_map<trx_hash_t, Transaction> moveVerifiedTrxSnapShot();
  std::unordered_map<trx_hash_t, Transaction> getNewVerifiedTrxSnapShot(
      bool onlyNew);
  std::unordered_map<trx_hash_t, Transaction> removeBlockTransactionsFromQueue(
      vec_trx_t const &allBlockTransactions);
  unsigned long getVerifiedTrxCount();
  void setVerifyMode(VerifyMode mode) { mode_ = mode; }
  std::shared_ptr<Transaction> getTransaction(trx_hash_t const &hash);

 private:
  using uLock = std::unique_lock<std::mutex>;
  using listIter = std::list<Transaction>::iterator;
  void verifyTrx();
  bool stopped_ = true;
  VerifyMode mode_ = VerifyMode::normal;
  bool new_verified_transactions = true;
  size_t num_verifiers_ = 2;
  TransactionStatusTable &trx_status_;
  unsigned current_capacity_ = 1024;
  unsigned future_capacity_ = 1024;

  std::list<Transaction> trx_buffer_;
  std::unordered_map<trx_hash_t, listIter> queued_trxs_;  // all trx
  std::mutex mutex_for_queued_trxs_;

  std::unordered_map<trx_hash_t, listIter> verified_trxs_;
  std::mutex mutex_for_verified_qu_;

  std::deque<std::pair<trx_hash_t, listIter>> unverified_hash_qu_;
  std::mutex mutex_for_unverified_qu_;
  std::condition_variable cond_for_unverified_qu_;

  std::vector<std::thread> verifiers_;

  dev::Logger log_er_{
      dev::createLogger(dev::Verbosity::VerbosityError, "TRXQU")};
  dev::Logger log_wr_{
      dev::createLogger(dev::Verbosity::VerbosityWarning, "TRXQU")};
  dev::Logger log_nf_{
      dev::createLogger(dev::Verbosity::VerbosityInfo, "TRXQU")};
  dev::Logger log_dg_{
      dev::createLogger(dev::Verbosity::VerbosityDebug, "TRXQU")};
};

/**
 * Manage transactions within an epoch
 * 1. Check existence of transaction (seen in queue? in db?)
 * 2. Push to transaction queue and verify
 *
 */

class TransactionManager
    : public std::enable_shared_from_this<TransactionManager> {
 public:
  using uLock = std::unique_lock<std::mutex>;
  enum class MgrStatus : uint8_t { idle, verifying, proposing };
  enum class VerifyMode : uint8_t { normal, skip_verify_sig };

  TransactionManager(std::shared_ptr<SimpleDBFace> db_trx)
      : db_trxs_(db_trx),
        trx_status_(),
        trx_qu_(trx_status_, 8 /*num verifiers*/) {
    trx_qu_.start();
  }
  std::shared_ptr<TransactionManager> getShared() {
    try {
      return shared_from_this();
    } catch (std::bad_weak_ptr &e) {
      LOG(log_er_) << e.what() << std::endl;
      return nullptr;
    }
  }
  virtual ~TransactionManager() {
    if (!stopped_) stop();
  }
  void start() {
    if (!stopped_) return;
    stopped_ = false;
  }
  void stop() {
    if (stopped_) return;
    stopped_ = true;
    cond_for_pack_trx_.notify_all();
  }
  bool insertTrx(Transaction trx, bool critical);
  void setPackedTrxFromBlock(DagBlock const &dag_block);
  void setPackedTrxFromBlockHash(blk_hash_t blk);
  /**
   * The following function will require a lock for verified qu
   */
  void packTrxs(vec_trx_t &to_be_packed_trx);
  void setVerifyMode(VerifyMode mode) {
    mode_ = mode;
    trx_qu_.setVerifyMode(TransactionQueue::VerifyMode::skip_verify_sig);
  }

  std::unordered_map<trx_hash_t, Transaction> getNewVerifiedTrxSnapShot(
      bool onlyNew);
  // Received block means some trx might be packed by others
  bool saveBlockTransactionsAndUpdateTransactionStatus(
      vec_trx_t const &all_block_trx_hashes,
      std::vector<Transaction> const &some_trxs);
  std::shared_ptr<Transaction> getTransaction(trx_hash_t const &hash);
  bool isTransactionVerified(trx_hash_t const &hash) {
    // in_block means in db, i.e., already verified
    auto status = trx_status_.get(hash);
    return status.second ? status.first == TransactionStatus::in_block : false;
  }

 private:
  MgrStatus mgr_status_ = MgrStatus::idle;
  VerifyMode mode_ = VerifyMode::normal;
  bool stopped_ = true;

  std::shared_ptr<SimpleDBFace> db_trxs_;
  TransactionStatusTable trx_status_;
  TransactionQueue trx_qu_;
  std::vector<std::thread> worker_threads_;
  std::mutex mutex_;

  std::mutex mutex_for_pack_trx_;
  std::condition_variable cond_for_pack_trx_;
  dev::Logger log_er_{
      dev::createLogger(dev::Verbosity::VerbosityError, "TRXMGR")};
  dev::Logger log_wr_{
      dev::createLogger(dev::Verbosity::VerbosityWarning, "TRXMGR")};
  dev::Logger log_nf_{
      dev::createLogger(dev::Verbosity::VerbosityInfo, "TRXMGR")};
  dev::Logger log_dg_{
      dev::createLogger(dev::Verbosity::VerbosityDebug, "TRXMGR")};
};

}  // namespace taraxa

#endif
