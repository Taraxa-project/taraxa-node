#pragma once

#include "common/constants.hpp"
#include "transaction/transaction.hpp"

namespace taraxa {
struct SystemTransaction : public Transaction {
  SystemTransaction(const trx_nonce_t &nonce, const val_t &value, const val_t &gas_price, gas_t gas, bytes data,
                    const std::optional<addr_t> &receiver = std::nullopt, uint64_t chain_id = 0);

  explicit SystemTransaction(const dev::RLP &_rlp, bool verify_strict = false, const h256 &hash = {})
      : Transaction(_rlp, verify_strict, hash) {
    sender_ = kTaraxaSystemAccount;
  };
  explicit SystemTransaction(const bytes &_rlp, bool verify_strict = false, const h256 &hash = {})
      : Transaction(_rlp, verify_strict, hash) {
    sender_ = kTaraxaSystemAccount;
  };

  virtual const addr_t &getSender() const override;
};

}  // namespace taraxa
