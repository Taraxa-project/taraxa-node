#pragma once

#include <libp2p/Common.h>

#include <array>
#include <chrono>
#include <shared_mutex>
#include <tuple>
#include <unordered_map>

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
  BandwidthStats(std::chrono::seconds bandwidth_throttle_period_duration, uint64_t max_allowed_total_packets_size_,
                 size_t max_allowed_total_packets_count_, uint64_t max_allowed_same_type_packets_size,
                 size_t max_allowed_same_type_packets_count);
  ~BandwidthStats() = default;

  /**
   * @brief Adds packet bandwidth data into the node's stats and Checks if node bandwidth is exceeded
   *
   * @param node_id
   * @param packet_type
   * @param packet_size
   * @return <true, "reason"> if max allowed bandwidth is exceeded, otherwise <false, "">
   */
  std::pair<bool, std::string> isExceeded(const dev::p2p::NodeID& node_id, SubprotocolPacketType packet_type,
                                          uint64_t packet_size);

 private:
  /**
   * @brief packet type bandwidth stats
   */
  struct PacketTypeStats {
    size_t packets_count_{0};
    uint64_t packets_size_{0};  // [Bytes]
  };

  /**
   * @brief node bandwidth stats
   */
  struct NodeStats {
    NodeStats() : bandwidth_throttle_period_start_(std::chrono::steady_clock::now()) {}

    // Beginning of time period for which stats are collected
    std::chrono::steady_clock::time_point bandwidth_throttle_period_start_;

    // total count of all received packets(all types)
    size_t total_packets_count_{0};

    // total size of all received packets(all types) [Bytes]
    uint64_t total_packets_size_{0};

    // count/size stats per packet type
    std::array<PacketTypeStats, SubprotocolPacketType::PacketCount> packets_types_stats_{};
  };

 private:
  // period duration during which bandwidth stats are relevant.
  // After this period duration passes, stats are reset to zero
  const std::chrono::seconds k_bandwidth_throttle_period_duration_;

  // max allowed received packets size of all types per k_bandwidth_throttle_period_duration_ [Bytes]
  const uint64_t k_max_allowed_total_packets_size_;

  // max allowed received packets count of all types per k_bandwidth_throttle_period_duration_
  const size_t k_max_allowed_total_packets_count_;

  // max allowed received packets size of one specific type per k_bandwidth_throttle_period_duration_ [Bytes]
  const uint64_t k_max_allowed_same_type_packets_size_;

  // max allowed received packets count of one specific type per k_bandwidth_throttle_period_duration_
  const size_t k_max_allowed_same_type_packets_count_;

  // bandwidth stats per nodeID
  std::unordered_map<dev::p2p::NodeID, NodeStats> stats_;
  std::shared_mutex stats_mutex_;
};

}  // namespace taraxa::network::tarcap
