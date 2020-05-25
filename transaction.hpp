#ifndef TARAXA_NODE_TRANSACTION_HPP
#define TARAXA_NODE_TRANSACTION_HPP

#include <libdevcore/Log.h>
#include <libdevcore/RLP.h>
#include <libdevcore/SHA3.h>
#include <libdevcrypto/Common.h>
#include <libethcore/Common.h>

#include <atomic>
#include <boost/thread/condition_variable.hpp>
#include <condition_variable>
#include <iostream>
#include <list>
#include <queue>
#include <thread>
#include <vector>

#include "dag_block.hpp"
#include "db_storage.hpp"
#include "util.hpp"

namespace taraxa {

using std::string;
class DagBlock;
class FullNode;

enum class TransactionStatus {
  invalid = 0,
  in_block,  // confirmed state, inside of block created by us or someone else
  in_queue_unverified,
  in_queue_verified,
  not_seen
};

class Transaction {
 public:
  enum class Type : uint8_t { Null, Create, Call };
  Transaction() = default;
  // default constructor
  Transaction(trx_hash_t const &hash, Type type, val_t const &nonce,
              val_t const &value, val_t const &gas_price, val_t const &gas,
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
  Transaction(val_t const &nonce, val_t const &value, val_t const &gas_price,
              val_t const &gas, bytes const &data, secret_t const &sk,
              optional<addr_t> const &receiver = std::nullopt) try
      : type_(receiver ? Transaction::Type::Call : Transaction::Type::Create),
        nonce_(nonce),
        value_(value),
        gas_price_(gas_price),
        gas_(gas),
        receiver_(receiver ? *receiver : addr_t()),
        data_(data) {
    sign(sk);
    updateHash();
  } catch (std::exception &e) {
    std::cerr << e.what() << std::endl;
  }

  Transaction(Transaction &&trx) = default;
  Transaction(Transaction const &trx) = default;
  explicit Transaction(stream &strm);
  explicit Transaction(string const &json);
  explicit Transaction(bytes const &_rlp);
  trx_hash_t getHash() const { return hash_; }
  Type getType() const { return type_; }
  val_t getNonce() const { return nonce_; }
  val_t getValue() const { return value_; }
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
    strm << "  sender: " << trans.sender() << std::endl;

    return strm;
  }
  bool serialize(stream &strm) const;
  bool deserialize(stream &strm);
  string getJsonStr() const;
  Json::Value getJson() const;
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
  bool hasSig() const { return vrs_.is_initialized(); }
  bool hasZeroSig() const { return vrs_ && isZeroSig(vrs_->r, vrs_->s); }
  bool isZeroSig(val_t const &r, val_t const &s) const { return !r && !s; }
  // Serialises this transaction to an RLPStream.
  void streamRLP(dev::RLPStream &s, bool include_sig = true,
                 bool _forEip155hash = false) const;
  // @returns the RLP serialisation of this transaction.
  bytes rlp(bool include_sig) const;

 protected:
  // @returns the SHA3 hash of the RLP serialisation of this transaction.
  blk_hash_t sha3(bool include_sig) const;
  trx_hash_t hash_;
  Type type_ = Type::Null;
  val_t nonce_ = 0;
  val_t value_ = 0;
  val_t gas_price_;
  val_t gas_;
  addr_t receiver_;
  sig_t sig_;
  bytes data_;
  boost::optional<dev::SignatureStruct> vrs_;
  int chain_id_ = -4;
  mutable addr_t cached_sender_;  ///< Cached sender, determined from signature.
};
}  // namespace taraxa

#endif