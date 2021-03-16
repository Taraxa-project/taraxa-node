#include "packets_stats.hpp"

namespace taraxa {

PacketsStats::PacketsStats(SubprotocolPacketType packetsCount) : packets_count_(packetsCount) { clearData(); }

void PacketsStats::addPacket(PacketType packet_type, const PacketStats &packet) {
  auto &packet_stats = stats_[packet_type];

  packet_stats.total_count_++;
  packet_stats.total_size_ += packet.size_;
  packet_stats.total_duration_ += packet.total_duration_;
}

void PacketsStats::clearData() {
  for (PacketType idx = 0; idx < static_cast<PacketType>(packets_count_); idx++) {
    stats_[idx].clearData();
  }
}

void PacketsStats::PacketAvgStats::clearData() {
  total_count_ = 0;
  total_size_ = 0;
  total_duration_ = std::chrono::microseconds(0);
}

std::ostream &operator<<(std::ostream &os, const PacketStats &stats) {
  os << "node: " << stats.node_.abridged() << ", size: " << stats.size_ << " [B]"
     << ", processing duration: " << stats.total_duration_.count() << " [us]";
  return os;
}

std::ostream &operator<<(std::ostream &os, const PacketsStats::PacketAvgStats &stats) {
  auto divisor = stats.total_count_ ? stats.total_count_ : 1;

  os << "total_count_: " << stats.total_count_ << ", total_size_: " << stats.total_size_ << " [B]"
     << ", avg_size_: " << stats.total_size_ / divisor << " [B]"
     << ", total_duration_: " << stats.total_duration_.count() << " [us]"
     << ", avg total_duration_: " << stats.total_duration_.count() / divisor << " [us]";
  return os;
}

std::ostream &operator<<(std::ostream &os, const PacketsStats &packets_stats) {
  os << "[";
  size_t idx = 0;
  for (const auto &stats : packets_stats.stats_) {
    if (stats.second.total_count_ == 0) {
      continue;
    }

    if (idx > 0) {
      os << ",";
    }

    os << "[" << packetToPacketName(stats.first) << " -> " << stats.second << "]";
    idx++;
  }
  os << "]";

  return os;
}

}  // namespace taraxa
