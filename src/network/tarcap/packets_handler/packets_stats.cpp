#include "packets_stats.hpp"

namespace taraxa::network::tarcap {

PacketsStats::PacketsStats(const PacketsStats &ro) : stats_(ro.stats_), mutex_() {}

PacketsStats &PacketsStats::operator=(const PacketsStats &ro) {
  stats_ = ro.stats_;
  return *this;
}

void PacketsStats::addPacket(const std::string &packet_type, const PacketStats &packet) {
  std::lock_guard<std::mutex> guard(mutex_);
  auto &packet_stats = stats_[packet_type];

  if (packet.is_unique_) {
    packet_stats.total_unique_count_++;
    packet_stats.total_unique_size_ += packet.size_;
    packet_stats.total_unique_duration_ += packet.total_duration_;
  }

  packet_stats.total_count_++;
  packet_stats.total_size_ += packet.size_;
  packet_stats.total_duration_ += packet.total_duration_;
}

std::optional<PacketsStats::PacketAvgStats> PacketsStats::getPacketStats(const std::string &packet_type) const {
  const auto found_stats = stats_.find(packet_type);

  if (found_stats == stats_.end()) {
    return std::nullopt;
  }

  return {found_stats->second};
}

PacketsStats::PacketAvgStats PacketsStats::PacketAvgStats::operator-(const PacketAvgStats &ro) const {
  PacketAvgStats result;

  result.total_count_ = total_count_ - ro.total_count_;
  result.total_size_ = total_size_ - ro.total_size_;
  result.total_duration_ = total_duration_ - ro.total_duration_;
  result.total_unique_count_ = total_unique_count_ - ro.total_unique_count_;
  result.total_unique_size_ = total_unique_size_ - ro.total_unique_size_;
  result.total_unique_duration_ = total_unique_duration_ - ro.total_unique_duration_;

  return result;
}

std::ostream &operator<<(std::ostream &os, const PacketStats &stats) {
  os << "node: " << stats.node_.abridged() << ", size: " << stats.size_ << " [B]"
     << ", processing duration: " << stats.total_duration_.count() << " [us]"
     << ", is unique: " << stats.is_unique_;

  return os;
}

std::ostream &operator<<(std::ostream &os, const PacketsStats::PacketAvgStats &stats) {
  auto divisor = stats.total_count_ ? stats.total_count_ : 1;

  os << "total_count_: " << stats.total_count_ << ", total_size_: " << stats.total_size_ << " [B]"
     << ", avg_size_: " << stats.total_size_ / divisor << " [B]"
     << ", total_duration_: " << stats.total_duration_.count() << " [us]"
     << ", avg total_duration_: " << stats.total_duration_.count() / divisor << " [us]";

  // Most packet dont support total_unique_count_, print this stats only for those, who have some unique packets
  // registered
  if (stats.total_unique_count_ > 0) {
    auto unique_divisor = stats.total_unique_count_ ? stats.total_unique_count_ : 1;
    os << ", total_unique_count_: " << stats.total_unique_count_ << ", total_unique_size_: " << stats.total_unique_size_
       << " [B]"
       << ", avg_unique_size_: " << stats.total_unique_size_ / unique_divisor << " [B]"
       << ", total_unique_duration_: " << stats.total_unique_duration_.count() << " [us]"
       << ", avg total_unique_duration_: " << stats.total_unique_duration_.count() / unique_divisor << " [us]";
  }

  return os;
}

PacketsStats PacketsStats::operator-(const PacketsStats &ro) const {
  PacketsStats result;

  for (const auto &packet_stats : stats_) {
    const auto ro_packet_stats = ro.getPacketStats(packet_stats.first);

    if (ro_packet_stats == std::nullopt) {
      result.stats_[packet_stats.first] = packet_stats.second;
    } else {
      PacketAvgStats packet_avg_stats_result = packet_stats.second - ro_packet_stats.value();
      if (packet_avg_stats_result.total_count_ > 0) {
        result.stats_[packet_stats.first] = std::move(packet_avg_stats_result);
      }
    }
  }

  return result;
}

std::ostream &operator<<(std::ostream &os, const PacketsStats &packets_stats) {
  os << "[";
  size_t idx = 0;
  for (const auto &stats : packets_stats.stats_) {
    if (idx > 0) {
      os << ",";
    }

    os << "[" << stats.first << " -> " << stats.second << "]";
    idx++;
  }
  os << "]";

  return os;
}

}  // namespace taraxa::network::tarcap