#include "network/tarcap/stats/packets_avg_stats.hpp"

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
    Json::Value packet_json = single_packet_stats.second.getStatsJson(include_duration_fields);
    packet_json["type"] = std::move(single_packet_stats.first);
    ret.append(std::move(packet_json));
  }

  return ret;
}

void AllPacketTypesStats::updatePeriodMaxStats(const AllPacketTypesStats &period_stats) {
  std::scoped_lock<std::shared_mutex> lock(max_stats_mutex_);
  for (const auto &packet_stats : period_stats.getStatsCopy()) {
    auto &abs_max_packet_count = max_counts_stats_[packet_stats.first];
    if (packet_stats.second.count_ > abs_max_packet_count.count_) {
      abs_max_packet_count = packet_stats.second;
    }

    auto &abs_max_packet_size = max_sizes_stats_[packet_stats.first];
    if (packet_stats.second.size_ > abs_max_packet_size.size_) {
      abs_max_packet_size = packet_stats.second;
    }
  }
}

Json::Value AllPacketTypesStats::getPeriodMaxStatsJson(bool include_duration_fields) const {
  Json::Value max_counts_stats_json;
  Json::Value max_sizes_stats_json;

  {
    std::shared_lock<std::shared_mutex> lock(max_stats_mutex_);
    for (const auto &packet : max_counts_stats_) {
      Json::Value packet_json = packet.second.getStatsJson(include_duration_fields /* include duration fields */);
      packet_json["type"] = std::move(packet.first);
      max_counts_stats_json.append(std::move(packet_json));
    }

    for (const auto &packet : max_sizes_stats_) {
      Json::Value packet_json = packet.second.getStatsJson(include_duration_fields /* include duration fields */);
      packet_json["type"] = std::move(packet.first);
      max_sizes_stats_json.append(std::move(packet_json));
    }
  }

  Json::Value ret;
  ret["period_max_counts_stats"] = std::move(max_counts_stats_json);
  ret["period_max_sizes_stats"] = std::move(max_sizes_stats_json);

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

Json::Value AllPacketTypesStats::PacketTypeStats::getStatsJson(bool include_duration_fields) const {
  Json::Value ret;
  const auto divisor = count_ ? count_ : 1;

  ret["total_count"] = Json::UInt64(count_);
  ret["total_size"] = Json::UInt64(size_);
  ret["avg_size"] = Json::UInt64(size_ / divisor);

  if (include_duration_fields) {
    ret["total_processing_duration"] = Json::UInt64(processing_duration_.count());
    ret["total_tp_wait_duration"] = Json::UInt64(tp_wait_duration_.count());
    ret["avg_processing_duration"] = Json::UInt64(processing_duration_.count() / divisor);
    ret["avg_tp_wait_duration"] = Json::UInt64(tp_wait_duration_.count() / divisor);
  }

  return ret;
}

AllPacketTypesStats::PacketTypeStats AllPacketTypesStats::PacketTypeStats::operator-(const PacketTypeStats &ro) const {
  PacketTypeStats result;

  result.count_ = count_ - ro.count_;
  result.size_ = size_ - ro.size_;
  result.processing_duration_ = processing_duration_ - ro.processing_duration_;
  result.tp_wait_duration_ = tp_wait_duration_ - ro.tp_wait_duration_;

  return result;
}

Json::Value SinglePacketStats::getStatsJson(bool include_duration_fields) const {
  Json::Value ret;

  ret["node"] = Json::String(node_.abridged());
  ret["size"] = Json::UInt64(size_);

  if (include_duration_fields) {
    ret["processing_duration"] = Json::UInt64(processing_duration_.count());
    ret["tp_wait_duration"] = Json::UInt64(tp_wait_duration_.count());
  }

  return ret;
}

std::string SinglePacketStats::getStatsJsonStr(const std::string &packet_type, bool include_duration_fields) const {
  std::ostringstream ret;
  ret << "{\"type\":\"" << packet_type << "\",";
  ret << "\"size\":" << size_ << ",";
  ret << "\"node\":\"" << node_.abridged() << "\"";

  if (include_duration_fields) {
    ret << ",\"processing_duration\":" << processing_duration_.count() << ",";
    ret << "\"tp_wait_duration\":" << tp_wait_duration_.count();
  }

  ret << "}";

  return ret.str();
}

}  // namespace taraxa::network::tarcap