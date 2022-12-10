#include "network/tarcap/stats/max_stats.hpp"

namespace taraxa::network::tarcap {

Json::Value MaxStats::getMaxStatsJson() const {
  Json::Value ret;
  ret["max_count"] = max_count_stats_.getStatsJson(true);
  ret["max_size"] = max_size_stats_.getStatsJson(true);
  ret["max_processing_duration"] = max_processing_duration_stats_.getStatsJson(true);
  ret["max_tp_wait_time"] = max_tp_wait_time_stats_.getStatsJson(true);

  return ret;
}

void MaxStats::updateMaxStats(const PacketStats& packet_stats) {
  if (packet_stats.count_ > max_count_stats_.count_) {
    max_count_stats_ = packet_stats;
  }

  if (packet_stats.size_ > max_size_stats_.size_) {
    max_size_stats_ = packet_stats;
  }

  if (packet_stats.processing_duration_ > max_processing_duration_stats_.processing_duration_) {
    max_processing_duration_stats_ = packet_stats;
  }

  if (packet_stats.tp_wait_duration_ > max_tp_wait_time_stats_.tp_wait_duration_) {
    max_tp_wait_time_stats_ = packet_stats;
  }
}

}  // namespace taraxa::network::tarcap