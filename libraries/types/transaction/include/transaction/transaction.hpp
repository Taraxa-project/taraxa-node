#pragma once

#include <json/json.h>
#include <libdevcore/RLP.h>
#include <libdevcore/SHA3.h>

#include "common/default_construct_copyable_movable.hpp"
#include "common/types.hpp"

namespace taraxa {

struct Transaction {
  struct InvalidTransaction : std::runtime_error {
    explicit InvalidTransaction(const std::string &msg) : runtime_error("invalid transaction - " + msg) {}
  };

  struct InvalidSignature : InvalidTransaction {
    explicit InvalidSignature(const std::string &msg) : InvalidTransaction("signature:\n" + msg) {}
  };

  struct InvalidFormat : InvalidTransaction {
    explicit InvalidFormat(const std::string &msg) : InvalidTransaction("rlp format:\n" + msg) {}
  };

 protected:
  trx_nonce_t nonce_ = 0;
  val_t value_ = 0;
  val_t gas_price_;
  gas_t gas_ = 0;
  bytes data_;
  std::optional<addr_t> receiver_;
  uint64_t chain_id_ = 0;
  dev::SignatureStruct vrs_;
  mutable trx_hash_t hash_;
  mutable std::atomic_bool hash_initialized_ = false;
  bool is_zero_ = false;
  mutable std::mutex hash_mu_;
  mutable std::atomic_bool sender_initialized_ = false;
  mutable bool sender_valid_ = false;
  mutable addr_t sender_;
  mutable std::mutex sender_mu_;
  mutable std::atomic_bool cached_rlp_set_ = false;
  mutable bytes cached_rlp_;
  mutable std::mutex cached_rlp_mu_;

  trx_hash_t hash_for_signature() const;
  addr_t const &get_sender_() const;
  virtual void streamRLP(dev::RLPStream &s, bool for_signature) const;
  virtual void fromRLP(const dev::RLP &_rlp, bool verify_strict, const h256 &hash);

 public:
  // TODO eliminate and use shared_ptr<Transaction> everywhere
  Transaction() : is_zero_(true) {}
  Transaction(const trx_nonce_t &nonce, const val_t &value, const val_t &gas_price, gas_t gas, bytes data,
              const secret_t &sk, const std::optional<addr_t> &receiver = std::nullopt, uint64_t chain_id = 0);
  explicit Transaction(const dev::RLP &_rlp, bool verify_strict = false, const h256 &hash = {});
  explicit Transaction(const bytes &_rlp, bool verify_strict = false, const h256 &hash = {});

  auto isZero() const { return is_zero_; }
  const trx_hash_t &getHash() const;
  auto getNonce() const { return nonce_; }
  const auto &getValue() const { return value_; }
  const auto &getGasPrice() const { return gas_price_; }
  auto getGas() const { return gas_; }
  const auto &getData() const { return data_; }
  const auto &getReceiver() const { return receiver_; }
  auto getChainID() const { return chain_id_; }
  const auto &getVRS() const { return vrs_; }
  auto getCost() const { return gas_price_ * gas_ + value_; }

  virtual const addr_t &getSender() const;

  bool operator==(Transaction const &other) const { return getHash() == other.getHash(); }

  const bytes &rlp() const;

  Json::Value toJSON() const;
};

using SharedTransaction = std::shared_ptr<Transaction>;
using Transactions = std::vector<Transaction>;
using SharedTransactions = std::vector<SharedTransaction>;
using TransactionHashes = std::vector<trx_hash_t>;

TransactionHashes hashes_from_transactions(const SharedTransactions &transactions);

}  // namespace taraxa
