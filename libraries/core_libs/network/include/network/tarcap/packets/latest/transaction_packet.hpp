#pragma once

#include "transaction/transaction.hpp"

namespace taraxa::network::tarcap {

struct TransactionPacket {
  std::vector<std::shared_ptr<Transaction>> transactions;

  RLP_FIELDS_DEFINE_INPLACE(transactions)

  constexpr static uint32_t kMaxTransactionsInPacket{500};
};

}  // namespace taraxa::network::tarcap
