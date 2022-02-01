#include "network/tarcap/stats/bandwidth_stats.hpp"

namespace taraxa::network::tarcap {

BandwidthStats::BandwidthStats() : bandwidth_throttle_period_start_(std::chrono::steady_clock::now()) {}

void BandwidthStats::resetData() {
  total_packets_count_ = 0;
  total_packets_size_ = 0;
  std::memset(packets_types_stats_.data(), 0, packets_types_stats_.size());
  bandwidth_throttle_period_start_ = std::chrono::steady_clock::now();
}

std::pair<bool, std::string> BandwidthStats::isExceeded(SubprotocolPacketType packet_type, uint64_t packet_size,
                                                        const NetworkConfig& conf) {
  assert(packet_type >= 0 && packet_type < SubprotocolPacketType::PacketCount);

  uint64_t total_packets_size, type_packets_size;
  size_t total_packets_count, type_packets_count;

  {
    std::scoped_lock lock(stats_mutex_);

    auto current_period_duration = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - bandwidth_throttle_period_start_);

    // period duration passed, reset stats to zero
    if (current_period_duration > conf.bandwidth_throttle_period_seconds) {
      resetData();
    }

    // Adjust node bandwidth stats
    total_packets_count = ++total_packets_count_;
    total_packets_size = total_packets_size_ += packet_size;

    auto& packet_type_stats = packets_types_stats_[packet_type];
    type_packets_count = ++packet_type_stats.packets_count_;
    type_packets_size = packet_type_stats.packets_size_ += packet_size;
  }

  // Check if bandwidth is exceeded - aggregate all reasons in case it is
  bool bandwidth_exceeded = false;
  std::string reasons = "";
  if (total_packets_count > conf.max_allowed_total_packets_count) {
    bandwidth_exceeded = true;
    reasons += "max allowed total packets count exceeded: " + std::to_string(total_packets_count) + " > " +
               std::to_string(conf.max_allowed_total_packets_count) + ", \n";
  }

  if (total_packets_size > conf.max_allowed_total_packets_size) {
    bandwidth_exceeded = true;
    reasons += "max allowed total packets size exceeded: " + std::to_string(total_packets_size) + " > " +
               std::to_string(conf.max_allowed_total_packets_size) + " [Bytes], \n";
  }

  if (type_packets_count > conf.max_allowed_same_type_packets_count) {
    bandwidth_exceeded = true;
    reasons += "max allowed packet type " + convertPacketTypeToString(packet_type) +
               " count exceeded: " + std::to_string(type_packets_count) + " > " +
               std::to_string(conf.max_allowed_same_type_packets_count) + ", \n";
  }

  if (type_packets_size > conf.max_allowed_same_type_packets_size) {
    bandwidth_exceeded = true;
    reasons += "max allowed packet type " + convertPacketTypeToString(packet_type) +
               " size exceeded: " + std::to_string(type_packets_size) + " > " +
               std::to_string(conf.max_allowed_same_type_packets_size) + " [Bytes] \n";
  }

  return {bandwidth_exceeded, reasons};
}

}  // namespace taraxa::network::tarcap