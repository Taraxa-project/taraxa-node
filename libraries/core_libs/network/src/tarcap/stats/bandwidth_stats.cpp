#include "network/tarcap/stats/bandwidth_stats.hpp"

namespace taraxa::network::tarcap {

BandwidthStats::BandwidthStats(std::chrono::seconds bandwidth_throttle_period_duration,
                               size_t max_allowed_total_packets_size, size_t max_allowed_total_packets_count,
                               size_t max_allowed_same_type_packets_size, size_t max_allowed_same_type_packets_count)
    : k_bandwidth_throttle_period_duration_(bandwidth_throttle_period_duration),
      k_max_allowed_total_packets_size_(max_allowed_total_packets_size),
      k_max_allowed_total_packets_count_(max_allowed_total_packets_count),
      k_max_allowed_same_type_packets_size_(max_allowed_same_type_packets_size),
      k_max_allowed_same_type_packets_count_(max_allowed_same_type_packets_count) {}

std::pair<bool, std::string> BandwidthStats::isExceeded(const dev::p2p::NodeID& node_id,
                                                        SubprotocolPacketType packet_type, size_t packet_size) {
  size_t total_packets_count, total_packets_size, type_packets_count, type_packets_size;

  {
    std::scoped_lock lock(stats_mutex_);
    auto& node_stats = stats_[node_id];

    auto current_period_duration = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - node_stats.bandwidth_throttle_period_start_);

    // period duration passed, reset stats to zero
    if (current_period_duration > k_bandwidth_throttle_period_duration_) {
      node_stats = NodeStats();
    }

    // Adjust node bandwidth stats
    total_packets_count = ++node_stats.total_packets_count_;
    total_packets_size = node_stats.total_packets_size_ += packet_size;

    auto& packet_type_stats = node_stats.packets_types_stats_[packet_type];
    type_packets_count = ++packet_type_stats.packets_count_;
    type_packets_size = packet_type_stats.packets_size_ += packet_size;
  }

  // Check if bandwidth is exceeded - aggregate all reasons in case it is
  bool bandwidth_exceeded = false;
  std::string reasons = "";
  if (total_packets_count > k_max_allowed_total_packets_count_) {
    bandwidth_exceeded = true;
    reasons += "total packets count exceeded: " + std::to_string(total_packets_count) + " > " +
               std::to_string(k_max_allowed_total_packets_count_) + " \n";
  }

  if (total_packets_size > k_max_allowed_total_packets_size_) {
    bandwidth_exceeded = true;
    reasons += "total packets size exceeded: " + std::to_string(total_packets_size) + " > " +
               std::to_string(k_max_allowed_total_packets_size_) + " [Bytes] \n";
  }

  if (type_packets_count > k_max_allowed_same_type_packets_count_) {
    bandwidth_exceeded = true;
    reasons += "packet type " + convertPacketTypeToString(packet_type) +
               " count exceeded: " + std::to_string(type_packets_count) + " > " +
               std::to_string(k_max_allowed_same_type_packets_count_) + " \n";
  }

  if (type_packets_size > k_max_allowed_same_type_packets_size_) {
    bandwidth_exceeded = true;
    reasons += "packet type " + convertPacketTypeToString(packet_type) +
               " size exceeded: " + std::to_string(type_packets_size) + " > " +
               std::to_string(k_max_allowed_same_type_packets_size_) + " [Bytes] \n";
  }

  return {bandwidth_exceeded, reasons};
}

}  // namespace taraxa::network::tarcap