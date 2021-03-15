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
  total_duration_ = std::chrono::milliseconds(0);
}

std::ostream &operator<<(std::ostream &os, const PacketStats &stats) {
  const std::time_t t = std::chrono::system_clock::to_time_t(stats.time_);
  os << "node: " << stats.node_.toString() << ", size: " << stats.size_ << " [B]"
     << ", time: " << std::ctime(&t) << ", processing duration: " << stats.total_duration_.count() << " [us]";
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
  for (const auto &it : packets_stats.stats_) {
    if (it.second.total_count_ == 0) {
      continue;
    }

    os << packetToPacketName(it.first) << "-> " << it.second << std::endl;
  }
  return os;
}

}  // namespace taraxa
