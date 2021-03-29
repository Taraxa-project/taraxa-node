#pragma once

#include <ostream>
#include <vector>

#include "packet_debug_info.hpp"
#include "transaction_manager/transaction.hpp"

namespace taraxa {

class TransactionPacketDebugInfo : public PacketDebugInfo {
 public:
  // All times are in microseconds
  struct TxInsertTimes {
    uint64_t total;
    uint64_t rlp_transform;
    uint64_t part1;
    uint64_t part2;
    uint64_t part3;
    uint64_t part4;
    uint64_t part5;
    bool was_verified;
    Transaction tx;

    friend std::ostream &operator<<(std::ostream &os, const TxInsertTimes &insert_times) {
      os << "(" << std::to_string(insert_times.total) << ", " << std::to_string(insert_times.rlp_transform) << ", "
         << std::to_string(insert_times.part1) << ", " << std::to_string(insert_times.part2) << ", "
         << std::to_string(insert_times.part3) << ", " << std::to_string(insert_times.part4) << ", "
         << std::to_string(insert_times.part5) << ", " << std::to_string(insert_times.was_verified) << ")"
         << " -> tx: " << insert_times.tx.toJSON();

      return os;
    }
  };

 public:
  uint64_t txs_rlp_transform_duration;  // [us]
  std::vector<TxInsertTimes> txs_insert_times;

  void pushNewTxDbgInfo() { txs_insert_times.emplace_back(); }
  inline TxInsertTimes &actMeasuredTx() { return txs_insert_times.back(); }

  std::string debugInfoToString() const override {
    std::ostringstream os;

    os << "Txs rlp transform duration: " << txs_rlp_transform_duration << ", txs inserts: [";

    for (const auto &tx_times : txs_insert_times) {
      if (tx_times.total > 1000) {
        os << tx_times << ", ";
      }
    }
    os << "]";

    return os.str();
  }
};

}  // namespace taraxa