#pragma once

#include <ostream>

#include "packet_debug_info.hpp"

namespace taraxa {

class TransactionPacketDebugInfo : public PacketDebugInfo {
 public:
  std::chrono::microseconds txs_rlp_transform_duration;

 public:
  std::string debugInfoToString() const override {
    std::ostringstream os;

    os << "Txs rlp transform duration: " << txs_rlp_transform_duration.count();

    return os.str();
  }
};

}  // namespace taraxa