#pragma once

#include <libp2p/Common.h>

#include <chrono>

#include "json/value.h"

namespace taraxa::network::tarcap {

/**
 * @brief Stats single packet type
 */
class PacketStats {
 public:
  uint64_t count_{0};
  uint64_t size_{0};
  std::chrono::microseconds processing_duration_{0};
  std::chrono::microseconds tp_wait_duration_{0};

  std::string getStatsJsonStr(const std::string &packet_type, const dev::p2p::NodeID &node,
                              bool include_duration_fields = true) const;
  Json::Value getStatsJson(bool include_duration_fields = true) const;
  PacketStats operator-(const PacketStats &ro) const;
};

}  // namespace taraxa::network::tarcap