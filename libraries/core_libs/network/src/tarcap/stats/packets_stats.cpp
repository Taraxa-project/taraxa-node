#include "network/tarcap/stats/packets_stats.hpp"

namespace taraxa::network::tarcap {

PacketsStats::PacketsStats() : start_time_(std::chrono::system_clock::now()) {}

void PacketsStats::addPacket(const std::string &packet_type, const PacketStats &packet) {
  std::scoped_lock<std::shared_mutex> lock(mutex_);
  auto &packet_stats = per_packet_stats_[packet_type];

  packet_stats.count_++;
  packet_stats.size_ += packet.size_;
  packet_stats.processing_duration_ += packet.processing_duration_;
  packet_stats.tp_wait_duration_ += packet.tp_wait_duration_;

  all_packets_stats_.count_++;
  all_packets_stats_.size_ += packet.size_;
  all_packets_stats_.processing_duration_ += packet.processing_duration_;
  all_packets_stats_.tp_wait_duration_ += packet.tp_wait_duration_;
}

PacketStats PacketsStats::getAllPacketsStatsCopy() const {
  std::shared_lock<std::shared_mutex> lock(mutex_);
  return all_packets_stats_;
}

PacketsStats::PerPacketStatsMap PacketsStats::getPerPacketStatsCopy() const {
  std::shared_lock<std::shared_mutex> lock(mutex_);
  return per_packet_stats_;
}

void PacketsStats::resetStats() {
  std::shared_lock<std::shared_mutex> lock(mutex_);

  start_time_ = std::chrono::system_clock::now();
  all_packets_stats_ = PacketStats{};
  per_packet_stats_.clear();
}

Json::Value PacketsStats::getStatsJson(bool include_duration_fields) const {
  const auto end_time = std::chrono::system_clock::now();

  Json::Value ret;
  auto &packets_stats_json = ret["packets"] = Json::Value(Json::arrayValue);

  std::time_t end_time_tt = std::chrono::system_clock::to_time_t(end_time);
  std::tm end_time_tm = *std::localtime(&end_time_tt);

  std::stringstream end_time_point_str;
  end_time_point_str << std::put_time(&end_time_tm, "%Y-%m-%d_%H:%M:%S");
  ret["end_time"] = end_time_point_str.str();
  end_time_point_str.clear();

  std::stringstream start_time_point_str;

  std::shared_lock<std::shared_mutex> lock(mutex_);

  std::time_t start_time_tt = std::chrono::system_clock::to_time_t(start_time_);
  std::tm start_time_tm = *std::localtime(&start_time_tt);
  start_time_point_str << std::put_time(&start_time_tm, "%Y-%m-%d_%H:%M:%S");
  ret["start_time"] = start_time_point_str.str();

  ret["duration_ms"] = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time_).count();

  Json::Value packet_json = all_packets_stats_.getStatsJson(include_duration_fields);
  packet_json["type"] = "ALL_PACKETS_COMBINED";
  packets_stats_json.append(std::move(packet_json));

  for (auto &single_packet_stats : per_packet_stats_) {
    packet_json = single_packet_stats.second.getStatsJson(include_duration_fields);
    packet_json["type"] = single_packet_stats.first;
    packets_stats_json.append(std::move(packet_json));
  }

  return ret;
}

void PacketsStats::updatePeriodMaxStats(const PacketsStats::PerPacketStatsMap &period_stats) {
  std::scoped_lock<std::shared_mutex> lock(max_stats_mutex_);
  for (const auto &packet_stats : period_stats) {
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

Json::Value PacketsStats::getPeriodMaxStatsJson(bool include_duration_fields) const {
  Json::Value max_counts_stats_json;
  Json::Value max_sizes_stats_json;

  {
    std::shared_lock<std::shared_mutex> lock(max_stats_mutex_);
    for (const auto &packet : max_counts_stats_) {
      Json::Value packet_json = packet.second.getStatsJson(include_duration_fields /* include duration fields */);
      packet_json["type"] = packet.first;
      max_counts_stats_json.append(std::move(packet_json));
    }

    for (const auto &packet : max_sizes_stats_) {
      Json::Value packet_json = packet.second.getStatsJson(include_duration_fields /* include duration fields */);
      packet_json["type"] = packet.first;
      max_sizes_stats_json.append(std::move(packet_json));
    }
  }

  Json::Value ret;
  ret["period_max_counts_stats"] = std::move(max_counts_stats_json);
  ret["period_max_sizes_stats"] = std::move(max_sizes_stats_json);

  return ret;
}

PacketsStats::PerPacketStatsMap operator-(const PacketsStats::PerPacketStatsMap &lo,
                                          const PacketsStats::PerPacketStatsMap &ro) {
  PacketsStats::PerPacketStatsMap result;

  for (const auto &lo_packet_stats : lo) {
    const auto ro_packets_stats = ro.find(lo_packet_stats.first);

    if (ro_packets_stats != ro.end()) {
      PacketStats packet_stats = lo_packet_stats.second - ro_packets_stats->second;
      if (packet_stats.count_ > 0) {
        result[lo_packet_stats.first] = packet_stats;
      }
    } else {
      result[lo_packet_stats.first] = lo_packet_stats.second;
    }
  }

  return result;
}

}  // namespace taraxa::network::tarcap