#pragma once

#include "network/tarcap/packets_handlers/latest/common/exceptions.hpp"
#include "transaction/transaction.hpp"

namespace taraxa::network::tarcap::v4 {

struct TransactionPacket {
  TransactionPacket(const dev::RLP& packet_rlp) {
    auto items = packet_rlp.itemCount();
    if (items != kTransactionPacketItemCount) {
      throw InvalidRlpItemsCountException("TransactionPacket", items, kTransactionPacketItemCount);
    }
    auto hashes_count = packet_rlp[0].itemCount();
    auto trx_count = packet_rlp[1].itemCount();

    if (hashes_count < trx_count) {
      throw InvalidRlpItemsCountException("TransactionPacket", hashes_count, trx_count);
    }
    if (hashes_count == 0 || hashes_count > kMaxTransactionsInPacket + kMaxHashesInPacket) {
      throw InvalidRlpItemsCountException("TransactionPacket", hashes_count,
                                          kMaxTransactionsInPacket + kMaxHashesInPacket);
    }

    if (trx_count > kMaxTransactionsInPacket) {
      throw InvalidRlpItemsCountException("TransactionPacket", trx_count, kMaxTransactionsInPacket);
    }

    // TODO: these hashes do not make sense after separating parsing
    //    // First extract only transaction hashes
    //    for (const auto tx_hash_rlp : packet_rlp[0]) {
    //      auto trx_hash = tx_hash_rlp.toHash<trx_hash_t>();
    //      txs_hashes.emplace_back(std::move(trx_hash));
    //    }

    for (const auto tx_rlp : packet_rlp[1]) {
      try {
        auto tx = std::make_shared<Transaction>(tx_rlp.data().toBytes());
        transactions.emplace_back(std::move(tx));
      } catch (const Transaction::InvalidTransaction& e) {
        throw MaliciousPeerException("Unable to parse transaction: " + std::string(e.what()));
      }
    }
  };

  std::vector<std::shared_ptr<Transaction>> transactions;

  static constexpr uint32_t kTransactionPacketItemCount = 2;
  static constexpr uint32_t kMaxTransactionsInPacket{500};
  static constexpr uint32_t kMaxHashesInPacket{5000};
};

}  // namespace taraxa::network::tarcap::v4
