#include "packets_avg_stats.hpp"

namespace taraxa::network::tarcap {

AllPacketTypesStats::AllPacketTypesStats(const AllPacketTypesStats &ro) : stats_(ro.stats_), mutex_() {}

AllPacketTypesStats &AllPacketTypesStats::operator=(const AllPacketTypesStats &ro) {
  std::scoped_lock<std::shared_mutex> lock(mutex_);
  stats_ = ro.getStatsCopy();

  return *this;
}

void AllPacketTypesStats::addPacket(const std::string &packet_type, const SinglePacketStats &packet) {
  std::scoped_lock<std::shared_mutex> lock(mutex_);
  auto &packet_stats = stats_[packet_type];

  packet_stats.count_++;
  packet_stats.size_ += packet.size_;
  packet_stats.processing_duration_ += packet.processing_duration_;
  packet_stats.tp_wait_duration_ += packet.tp_wait_duration_;
}

std::optional<AllPacketTypesStats::PacketTypeStats> AllPacketTypesStats::getPacketTypeStats(
    const std::string &packet_type) const {
  std::shared_lock<std::shared_mutex> lock(mutex_);
  const auto found_stats = stats_.find(packet_type);

  if (found_stats == stats_.end()) {
    return {};
  }

  return {found_stats->second};
}

std::unordered_map<std::string, AllPacketTypesStats::PacketTypeStats> AllPacketTypesStats::getStatsCopy() const {
  std::shared_lock<std::shared_mutex> lock(mutex_);
  return stats_;
}

Json::Value AllPacketTypesStats::getStatsJson(bool include_duration_fields) const {
  Json::Value ret;

  for (const auto &single_packet_stats : getStatsCopy()) {
    Json::Value packet_json;

    const auto &packet_stats = single_packet_stats.second;
    const auto divisor = packet_stats.count_ ? packet_stats.count_ : 1;

    packet_json["name"] = std::move(single_packet_stats.first);
    packet_json["total_count"] = Json::UInt64(packet_stats.count_);
    packet_json["total_size"] = Json::UInt64(packet_stats.size_);
    packet_json["avg_size"] = Json::UInt64(packet_stats.size_ / divisor);

    if (include_duration_fields) {
      packet_json["total_processing_duration"] = Json::UInt64(packet_stats.processing_duration_.count());
      packet_json["total_tp_wait_duration"] = Json::UInt64(packet_stats.tp_wait_duration_.count());
      packet_json["avg_processing_duration"] = Json::UInt64(packet_stats.processing_duration_.count() / divisor);
      packet_json["avg_tp_wait_duration"] = Json::UInt64(packet_stats.tp_wait_duration_.count() / divisor);
    }

    ret.append(std::move(packet_json));
  }

  return ret;
}

AllPacketTypesStats AllPacketTypesStats::operator-(const AllPacketTypesStats &ro) const {
  AllPacketTypesStats result;

  for (const auto &packet_stats : getStatsCopy()) {
    const auto ro_packet_stats = ro.getPacketTypeStats(packet_stats.first);

    if (ro_packet_stats.has_value()) {
      PacketTypeStats packet_avg_stats_result = packet_stats.second - ro_packet_stats.value();
      if (packet_avg_stats_result.count_ > 0) {
        result.stats_[packet_stats.first] = std::move(packet_avg_stats_result);
      }
    } else {
      result.stats_[packet_stats.first] = std::move(packet_stats.second);
    }
  }

  return result;
}

std::ostream &operator<<(std::ostream &os, const AllPacketTypesStats &packets_stats) {
  os << "[";
  size_t idx = 0;
  for (const auto &stats : packets_stats.getStatsCopy()) {
    if (idx > 0) {
      os << ",";
    }

    os << "[" << stats.first << " -> " << stats.second << "]";
    idx++;
  }
  os << "]";

  return os;
}

AllPacketTypesStats::PacketTypeStats AllPacketTypesStats::PacketTypeStats::operator-(const PacketTypeStats &ro) const {
  PacketTypeStats result;

  result.count_ = count_ - ro.count_;
  result.size_ = size_ - ro.size_;
  result.processing_duration_ = processing_duration_ - ro.processing_duration_;
  result.tp_wait_duration_ = tp_wait_duration_ - ro.tp_wait_duration_;

  return result;
}

std::ostream &operator<<(std::ostream &os, const SinglePacketStats &stats) {
  os << "node: " << stats.node_.abridged() << ", size: " << stats.size_ << " [B]"
     << ", processing duration: " << stats.processing_duration_.count() << " [us]"
     << ", threadpool wait duration: " << stats.tp_wait_duration_.count() << " [us]";

  return os;
}

std::ostream &operator<<(std::ostream &os, const AllPacketTypesStats::PacketTypeStats &stats) {
  auto divisor = stats.count_ ? stats.count_ : 1;

  os << "total_count: " << stats.count_ << ", total_size: " << stats.size_ << " [B]"
     << ", avg_size: " << stats.size_ / divisor << " [B]"
     << ", processing_duration: " << stats.processing_duration_.count() << " [us]"
     << ", avg processing_duration: " << stats.processing_duration_.count() / divisor << " [us]"
     << ", tp_wait_duration: " << stats.tp_wait_duration_.count() << " [us]"
     << ", avg tp_wait_duration: " << stats.tp_wait_duration_.count() / divisor << " [us]";

  return os;
}

}  // namespace taraxa::network::tarcap