#pragma once

#include <libp2p/Common.h>

#include <array>
#include <chrono>
#include <shared_mutex>
#include <tuple>
#include <unordered_map>

#include "config/config.hpp"
#include "network/tarcap/packet_types.hpp"

namespace taraxa::network::tarcap {

/**
 * @class BandwidthStats
 *
 * @brief BandwidthStats is supposed to collect data(number of packets, overall size of packets) for each node and
 *        limit their bandwidth based on it
 */
class BandwidthStats {
 public:
  BandwidthStats();
  ~BandwidthStats() = default;

  /**
   * @brief Adds packet bandwidth data into the node's stats and Checks if node bandwidth is exceeded
   *
   * @param packet_type
   * @param packet_size
   * @param conf
   * @return <true, "reason"> if max allowed bandwidth is exceeded, otherwise <false, "">
   */
  std::pair<bool, std::string> isExceeded(SubprotocolPacketType packet_type, uint64_t packet_size,
                                          const NetworkConfig &conf);

 private:
  /**
   * @brief packet type bandwidth stats
   */
  struct PacketTypeStats {
    size_t packets_count_{0};
    uint64_t packets_size_{0};  // [Bytes]
  };

  /**
   * @brief Resets data to zero
   */
  void resetData();

 private:
  // Beginning of time period for which stats are collected
  std::chrono::steady_clock::time_point bandwidth_throttle_period_start_;

  // total count of all received packets(all types)
  size_t total_packets_count_{0};

  // total size of all received packets(all types) [Bytes]
  uint64_t total_packets_size_{0};

  // count/size stats per packet type
  std::array<PacketTypeStats, SubprotocolPacketType::PacketCount> packets_types_stats_{};

  std::shared_mutex stats_mutex_;
};

}  // namespace taraxa::network::tarcap
