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

void PacketsStats::resetStats() {
  std::shared_lock<std::shared_mutex> lock(mutex_);

  start_time_ = std::chrono::system_clock::now();
  all_packets_stats_ = PacketStats{};
  per_packet_stats_.clear();
}

Json::Value PacketsStats::getStatsJson() const {
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

  Json::Value packet_json = all_packets_stats_.getStatsJson();
  packet_json["type"] = "ALL_PACKETS_COMBINED";
  packets_stats_json.append(std::move(packet_json));

  for (auto &single_packet_stats : per_packet_stats_) {
    packet_json = single_packet_stats.second.getStatsJson();
    packet_json["type"] = single_packet_stats.first;
    packets_stats_json.append(std::move(packet_json));
  }

  return ret;
}

Json::Value PacketsStats::getAllPacketsMaxStats() const { return all_packets_max_stats_.getMaxStatsJson(); }

void PacketsStats::updateAllPacketsMaxStats() { all_packets_max_stats_.updateMaxStats(getAllPacketsStatsCopy()); }

}  // namespace taraxa::network::tarcap