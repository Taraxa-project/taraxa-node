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
    uint64_t part1;
    uint64_t part2;
    uint64_t part3;
    uint64_t part4;
    uint64_t part5;
    uint64_t part6;
    Json::Value tx;
    std::string prosessing_steps;

    friend std::ostream &operator<<(std::ostream &os, const TxInsertTimes &insert_times) {
      std::string tx_json = insert_times.tx.toStyledString();
      tx_json.erase(std::remove(tx_json.begin(), tx_json.end(), '\n'), tx_json.end());

      os << "(" << std::to_string(insert_times.total) << ", " << std::to_string(insert_times.part1) << ", "
         << std::to_string(insert_times.part2) << ", " << std::to_string(insert_times.part3) << ", "
         << std::to_string(insert_times.part4) << ", " << std::to_string(insert_times.part5) << ", "
         << std::to_string(insert_times.part6) << " -> processing steps: {" << insert_times.prosessing_steps
         << "}, tx: " << tx_json << ")";

      return os;
    }
  };

 public:
  uint64_t txs_count{0};
  uint64_t txs_rlp_transform_duration;  // [us]
  std::vector<TxInsertTimes> txs_insert_times;

  void pushNewTxDbgInfo() {
    txs_insert_times.emplace_back();
    txs_count++;
  }
  inline TxInsertTimes &actMeasuredTx() { return txs_insert_times.back(); }

  std::string debugInfoToString() const override {
    std::ostringstream os;

    uint64_t tx_avg_total = 0;
    uint64_t tx_avg_part1 = 0;
    uint64_t tx_avg_part2 = 0;
    uint64_t tx_avg_part3 = 0;
    uint64_t tx_avg_part4 = 0;
    uint64_t tx_avg_part5 = 0;
    uint64_t tx_avg_part6 = 0;

    os << "Txs count: " << txs_count << ", rlp transform duration: " << txs_rlp_transform_duration
       << ", txs inserts(>= 1000us): [";

    for (const auto &tx_times : txs_insert_times) {
      if (tx_times.total >= 1000) {
        os << tx_times << ", ";
      }

      tx_avg_total += tx_times.total;
      tx_avg_part1 += tx_times.part1;
      tx_avg_part2 += tx_times.part2;
      tx_avg_part3 += tx_times.part3;
      tx_avg_part4 += tx_times.part4;
      tx_avg_part5 += tx_times.part5;
      tx_avg_part6 += tx_times.part6;
    }
    os << "], ";

    uint64_t divider = txs_insert_times.size() ? txs_insert_times.size() : 1;

    os << "Txs avg times: (" << tx_avg_total / divider << ", " << tx_avg_part1 / divider << ", "
       << tx_avg_part2 / divider << ", " << tx_avg_part3 / divider << ", " << tx_avg_part4 / divider << ", "
       << tx_avg_part5 / divider << ", " << tx_avg_part6 / divider << ")";

    return os.str();
  }
};

}  // namespace taraxa