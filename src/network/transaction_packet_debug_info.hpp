#pragma once

#include <ostream>
#include <vector>

#include "packet_debug_info.hpp"

namespace taraxa {

class TransactionPacketDebugInfo : public PacketDebugInfo {
 public:
  // All times are in microseconds
  struct TxInsertTimes {
    uint64_t total;
    uint64_t db_save_tx;
    uint64_t db_save_tx_status;
    uint64_t trx_qe_insert;

    friend std::ostream &operator<<(std::ostream &os, const TxInsertTimes &insert_times) {
      os << "(" << std::to_string(insert_times.total) << ", " << std::to_string(insert_times.db_save_tx) << ", "
         << std::to_string(insert_times.db_save_tx_status) << ", " << std::to_string(insert_times.trx_qe_insert);

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

    os << "Txs rlp transform duration: " << txs_rlp_transform_duration
       << ", txs insert durations (format [(total time [us], db_save_tx [us], db_save_tx_status [us], trx_qe_insert "
          "[us]),...]): [";

    for (const auto &tx_times : txs_insert_times) {
      os << tx_times << ", ";
    }
    os << "]";

    return os.str();
  }
};

}  // namespace taraxa