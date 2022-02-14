#pragma once

#include <optional>
#include <vector>

#include "common/encoding_rlp.hpp"

namespace taraxa {

/**
 * @brief Transaction packet data parsed from rlp
 */
struct TransactionPacketData {
  struct SingleTransaction {
    uint64_t nonce_;
    val_t gas_price_;
    uint64_t gas_;
    std::optional<addr_t> receiver_;
    val_t value_;
    bytes data_;
    u256 v, r, s;

    HAS_RLP_FIELDS
  };

  std::vector<SingleTransaction> transactions;

  RLP_FIELDS_DEFINE_INPLACE(transactions)
};

}  // namespace taraxa