#ifndef TARAXA_NODE_TRANSACTION_HPP
#define TARAXA_NODE_TRANSACTION_HPP

#include <libdevcore/RLP.h>
#include <libdevcore/SHA3.h>
#include <libdevcrypto/Common.h>
#include <libethcore/Common.h>

#include <optional>
#include <stdexcept>

#include "util.hpp"

namespace taraxa {

struct Transaction {
  struct InvalidChainID : std::invalid_argument {
    InvalidChainID() : invalid_argument("eip-155 chain id must be > 0") {}
  };
  struct InvalidEncodingSize : std::invalid_argument {
    InvalidEncodingSize()
        : invalid_argument("transaction rlp must be a list of 9 elements") {}
  };

 private:
  mutable bool sender_cached = false;
  mutable bool hash_cached = false;
  mutable trx_hash_t hash_;
  mutable addr_t sender_;
  uint64_t nonce_ = 0;
  val_t value_ = 0;
  val_t gas_price_;
  uint64_t gas_ = 0;
  bytes data_;
  std::optional<addr_t> receiver_;
  uint64_t chain_id = 0;
  dev::SignatureStruct vrs;
  mutable std::shared_ptr<bytes> cached_rlp;

  template <bool for_signature>
  void streamRLP(dev::RLPStream &s) const;
  trx_hash_t hash_for_signature() const;

 public:
  // TODO eliminate and use shared_ptr<Transaction> everywhere
  Transaction() = default;
  Transaction(uint64_t nonce, val_t const &value, val_t const &gas_price,
              uint64_t gas, bytes data, secret_t const &sk,
              std::optional<addr_t> const &receiver = std::nullopt,
              uint64_t chain_id = 0);
  explicit Transaction(bytes const &_rlp, bool verify_strict = false);
//  explicit Transaction(bytes &&rlp, bool verify_strict = false)
//      : Transaction(rlp, verify_strict) {
//    cached_rlp.reset(new auto(move(rlp)));
//  }

  trx_hash_t const &getHash() const;
  addr_t const &getSender() const;
  auto getNonce() const { return nonce_; }
  auto const &getValue() const { return value_; }
  auto const &getGasPrice() const { return gas_price_; }
  auto getGas() const { return gas_; }
  auto const &getData() const { return data_; }
  auto const &getReceiver() const { return receiver_; }
  auto getChainID() const { return chain_id; }
  auto const &getVRS() const { return vrs; }

  bool operator==(Transaction const &other) const {
    return getHash() == other.getHash();
  }

  std::shared_ptr<bytes> rlp(bool cache = false) const;
};

}  // namespace taraxa

#endif