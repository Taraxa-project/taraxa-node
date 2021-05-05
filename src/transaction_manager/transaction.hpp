#pragma once

#include <json/json.h>

#include "common/types.hpp"
#include "util/default_construct_copyable_movable.hpp"

namespace taraxa {

struct Transaction {
  struct InvalidChainID : std::invalid_argument {
    InvalidChainID() : invalid_argument("eip-155 chain id must be > 0") {}
  };
  struct InvalidEncodingSize : std::invalid_argument {
    InvalidEncodingSize() : invalid_argument("transaction rlp must be a list of 9 elements") {}
  };
  struct InvalidSignature : std::runtime_error {
    explicit InvalidSignature(std::string const &msg) : runtime_error("invalid signature:\n" + msg) {}
  };

 private:
  uint64_t nonce_ = 0;
  val_t value_ = 0;
  val_t gas_price_;
  uint64_t gas_ = 0;
  bytes data_;
  std::optional<addr_t> receiver_;
  uint64_t chain_id_ = 0;
  dev::SignatureStruct vrs_;
  mutable bool hash_initialized_ = false;
  mutable trx_hash_t hash_;
  mutable DefaultConstructCopyableMovable<std::mutex> hash_mu_;
  mutable bool sender_initialized_ = false;
  mutable bool sender_valid_ = false;
  mutable addr_t sender_;
  mutable DefaultConstructCopyableMovable<std::mutex> sender_mu_;
  mutable std::shared_ptr<bytes> cached_rlp_;
  mutable DefaultConstructCopyableMovable<std::mutex> cached_rlp_mu_;

  template <bool for_signature>
  void streamRLP(dev::RLPStream &s) const;
  trx_hash_t hash_for_signature() const;
  addr_t const &get_sender_() const;

 public:
  // TODO eliminate and use shared_ptr<Transaction> everywhere
  Transaction() = default;
  Transaction(uint64_t nonce, val_t const &value, val_t const &gas_price, uint64_t gas, bytes data, secret_t const &sk,
              std::optional<addr_t> const &receiver = std::nullopt, uint64_t chain_id = 0);
  explicit Transaction(dev::RLP const &_rlp, bool verify_strict = false);
  explicit Transaction(bytes const &_rlp, bool verify_strict = false) : Transaction(dev::RLP(_rlp), verify_strict) {}
  explicit Transaction(bytes &&_rlp, bool verify_strict = false) : Transaction(dev::RLP(_rlp), verify_strict) {
    cached_rlp_.reset(new bytes(std::move(_rlp)));
  }

  trx_hash_t const &getHash() const;
  addr_t const &getSender() const;
  auto getNonce() const { return nonce_; }
  auto const &getValue() const { return value_; }
  auto const &getGasPrice() const { return gas_price_; }
  auto getGas() const { return gas_; }
  auto const &getData() const { return data_; }
  auto const &getReceiver() const { return receiver_; }
  auto getChainID() const { return chain_id_; }
  auto const &getVRS() const { return vrs_; }

  bool operator==(Transaction const &other) const { return getHash() == other.getHash(); }

  std::shared_ptr<bytes> rlp(bool cache = false) const;

  Json::Value toJSON() const;
};

}  // namespace taraxa
