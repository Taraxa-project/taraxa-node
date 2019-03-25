/*
 * @Copyright: Taraxa.io
 * @Author: Chia-Chun Lin
 * @Date: 2019-02-27 12:27:18
 * @Last Modified by: Chia-Chun Lin
 * @Last Modified time: 2019-03-20 18:44:37
 */

#ifndef TRANSACTION_HPP
#define TRANSACTION_HPP
#include <condition_variable>
#include <iostream>
#include <list>
#include <queue>
#include <thread>
#include <vector>
#include "dag_block.hpp"
#include "libdevcore/Log.h"
#include "proto/taraxa_grpc.grpc.pb.h"
#include "rocks_db.hpp"
#include "types.hpp"
#include "util.hpp"

namespace taraxa {

using std::string;

enum class TransactionStatus {
  seen_but_invalid,
  seen_in_db,     // confirmed state, (packed)
  seen_in_queue,  // not packed yet
  seen_in_queue_but_already_packed_by_others,
  unseen,                              // not possible, won't store unseen state
  unseen_but_already_packed_by_others  // still need to be in queue to write to
                                       // db, but do not propose
};
/**
 * simple thread_safe hash
 * keep track of transaction state
 */

class TransactionStatusTable {
 public:
  using uLock = std::unique_lock<std::mutex>;
  std::pair<TransactionStatus, bool> get(trx_hash_t const &hash) {
    uLock lock(mutex_);
    auto iter = status_.find(hash);
    if (iter != status_.end()) {
      return {iter->second, true};
    } else {
      return {TransactionStatus::unseen, false};
    }
  }
  bool insert(trx_hash_t const &hash, TransactionStatus status) {
    uLock lock(mutex_);
    bool ret = false;
    if (status_.count(hash)) {
      ret = false;
    } else {
      status_[hash] = status;
      ret = true;
    }
    return ret;
  }
  bool compareAndSwap(trx_hash_t const &hash, TransactionStatus expected_value,
                      TransactionStatus new_value) {
    uLock lock(mutex_);
    auto iter = status_.find(hash);
    bool ret = false;
    if (iter == status_.end()) {
      ret = false;
    } else if (iter->second == expected_value) {
      iter->second = new_value;
      ret = true;
    }
    return ret;
  }

 private:
  std::mutex mutex_;
  std::unordered_map<trx_hash_t, TransactionStatus> status_;
};
class Transaction {
 public:
  enum class Type : uint8_t { Null, Creation, Call };
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
  Transaction(trx_hash_t const &hash, Type type, bal_t const &nonce,
              bal_t const &value, val_t const &gas_price, val_t const &gas,
              name_t const &receiver, sig_t const &sig, bytes const &data) try
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

  Transaction(Transaction &&trx) = default;
  Transaction(Transaction const &trx) = default;
  Transaction(stream &strm);
  Transaction(string const &json);
  trx_hash_t getHash() const { return hash_; }
  Type getType() const { return type_; }
  bal_t getNonce() const { return nonce_; }
  bal_t getValue() const { return value_; }
  val_t getGasPrice() const { return gas_price_; }
  val_t getGas() const { return gas_; }
  name_t getSender() const { return receiver_; }
  name_t getReceiver() const { return receiver_; }
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
    strm << "  sender: " << trans.sender_ << std::endl;
    strm << "  receiver: " << trans.receiver_ << std::endl;
    strm << "  data: " << bytes2str(trans.data_) << std::endl;
    return strm;
  }
  bool serialize(stream &strm) const;
  bool deserialize(stream &strm);
  string getJsonStr() const;
  bool isValid() const { return !hash_.isZero(); }
  bool operator==(Transaction const &other) const {
    return this->getJsonStr() == other.getJsonStr();
  }
  Transaction &operator=(Transaction &&other) = default;
  Transaction &operator=(Transaction const &other) = default;
  bool operator<(Transaction const &other) const { return hash_ < other.hash_; }

 protected:
  trx_hash_t hash_ = "";
  Type type_ = Type::Null;
  bal_t nonce_ = 0;
  bal_t value_ = 0;
  val_t gas_price_ = "";
  val_t gas_ = "";
  name_t sender_ = "";
  name_t receiver_ = "";
  sig_t sig_ = "";
  bytes data_;
};

/**
 * Thread safe
 * Sort transaction based on gas price
 * Firstly import unverified transaction to deque
 * Verifier threads will verify and move the transaction to verified priority
 */

class TransactionQueue {
 public:
  struct SizeLimit {
    size_t current = 1024;
    size_t future = 1024;
  };
  TransactionQueue(TransactionStatusTable &status, size_t num_verifiers)
      : trx_status_(status), num_verifiers_(num_verifiers) {}
  TransactionQueue(TransactionStatusTable &status, size_t num_verifiers,
                   unsigned current_capacity, unsigned future_capacity)
      : trx_status_(status),
        num_verifiers_(num_verifiers),
        current_capacity_(current_capacity),
        future_capacity_(future_capacity) {}
  ~TransactionQueue() { stop(); }
  void start();
  void stop();
  bool insert(Transaction trx);
  Transaction top();
  void pop();
  std::unordered_map<trx_hash_t, Transaction> moveVerifiedTrxSnapShot();

 private:
  using uLock = std::unique_lock<std::mutex>;

  void verifyTrx();
  bool stopped_ = true;
  size_t num_verifiers_ = 2;
  TransactionStatusTable &trx_status_;
  unsigned current_capacity_ = 1024;
  unsigned future_capacity_ = 1024;
  std::unordered_map<trx_hash_t, Transaction> verified_trxs_;
  std::deque<Transaction> unverified_qu_;
  std::vector<std::thread> verifiers_;
  std::mutex mutex_for_unverified_qu_;
  std::mutex mutex_for_verified_qu_;

  std::condition_variable cond_for_unverified_qu_;
  dev::Logger logger_{
      dev::createLogger(dev::Verbosity::VerbosityInfo, "trx_qu")};
  dev::Logger logger_dbg_{
      dev::createLogger(dev::Verbosity::VerbosityDebug, "trx_qu")};
};

/**
 * Manage transactions within an epoch
 * 1. Check existence of transaction (seen in queue? in db?)
 * 2. Push to transaction queue and verify
 *
 */

class TransactionManager {
 public:
  using uLock = std::unique_lock<std::mutex>;
  enum class MgrStatus : uint8_t { idle, verifying, proposing };
  TransactionManager(std::shared_ptr<RocksDb> db_block)
      : db_blocks_(db_block),
        db_trxs_(std::make_shared<RocksDb>("/tmp/rocksdb/trx")),
        trx_status_(),
        trx_qu_(trx_status_, 1 /*num verifiers*/) {
    trx_qu_.start();
  }
  bool insertTrx(Transaction trx);
  bool setPackedTrxFromBlock(DagBlock const &dag_block);
  bool setPackedTrxFromBlockHash(blk_hash_t blk);
  /**
   * The following function will require a lock for verified qu
   */
  bool packTrxs(vec_trx_t &to_be_packed_trx);

 private:
  MgrStatus mgr_status_ = MgrStatus::idle;
  std::shared_ptr<RocksDb> db_blocks_;
  std::shared_ptr<RocksDb> db_trxs_;
  TransactionStatusTable trx_status_;
  TransactionQueue trx_qu_;
  std::vector<std::thread> worker_threads_;
  std::mutex mutex_;
  dev::Logger logger_{
      dev::createLogger(dev::Verbosity::VerbosityInfo, "trx_qu")};
  dev::Logger logger_dbg_{
      dev::createLogger(dev::Verbosity::VerbosityDebug, "trx_qu")};
};

}  // namespace taraxa

#endif
