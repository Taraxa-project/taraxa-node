#pragma once

#include "transaction/transaction.hpp"

namespace taraxa::network::tarcap {

struct TransactionPacket {
  TransactionPacket() = default;
  TransactionPacket(const TransactionPacket&) = default;
  TransactionPacket(TransactionPacket&&) = default;
  TransactionPacket& operator=(const TransactionPacket&) = default;
  TransactionPacket& operator=(TransactionPacket&&) = default;
  // TODO[2868]: optimize ctor
  TransactionPacket(const std::vector<std::shared_ptr<Transaction>>& transactions) : transactions(transactions) {}
  TransactionPacket(const dev::RLP& packet_rlp) {
    *this = util::rlp_dec<TransactionPacket>(packet_rlp);
    if (transactions.size() > kMaxTransactionsInPacket) {
      throw InvalidRlpItemsCountException("TransactionPacket", transactions.size(), kMaxTransactionsInPacket);
    }
  }
  dev::bytes encodeRlp() { return util::rlp_enc(*this); }

  std::vector<std::shared_ptr<Transaction>> transactions;

  constexpr static uint32_t kTransactionPacketItemCount = 2;
  constexpr static uint32_t kMaxTransactionsInPacket{500};
  constexpr static uint32_t kMaxHashesInPacket{5000};

  RLP_FIELDS_DEFINE_INPLACE(transactions)
};

}  // namespace taraxa::network::tarcap
