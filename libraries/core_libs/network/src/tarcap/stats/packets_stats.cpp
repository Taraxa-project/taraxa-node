#include "network/tarcap/stats/packets_stats.hpp"

#include "common/util.hpp"
#include "json/writer.h"

namespace taraxa::network::tarcap {

PacketsStats::PacketsStats(const addr_t& node_addr) { LOG_OBJECTS_CREATE("NETPER"); }

void PacketsStats::addReceivedPacket(const std::string& packet_type, const SinglePacketStats& packet) {
  received_packets_stats_.addPacket(packet_type, packet);
  LOG(log_tr_) << "Received packet: " << packet.getStatsJsonStr(packet_type);
}

void PacketsStats::addSentPacket(const std::string& packet_type, const SinglePacketStats& packet) {
  sent_packets_stats_.addPacket(packet_type, packet);
  LOG(log_tr_) << "Sent packet: " << packet.getStatsJsonStr(packet_type);
}

const AllPacketTypesStats& PacketsStats::getSentPacketsStats() const { return sent_packets_stats_; }

const AllPacketTypesStats& PacketsStats::getReceivedPacketsStats() const { return received_packets_stats_; }

AllPacketTypesStats::PacketTypeStatsMap operator-(const AllPacketTypesStats::PacketTypeStatsMap& lo,
                                                  const AllPacketTypesStats::PacketTypeStatsMap& ro) {
  AllPacketTypesStats::PacketTypeStatsMap result;

  for (const auto& lo_packet_stats : lo) {
    const auto ro_packets_stats = ro.find(lo_packet_stats.first);

    if (ro_packets_stats != ro.end()) {
      AllPacketTypesStats::PacketTypeStats packet_avg_stats_result = lo_packet_stats.second - ro_packets_stats->second;
      if (packet_avg_stats_result.count_ > 0) {
        result[lo_packet_stats.first] = std::move(packet_avg_stats_result);
      }
    } else {
      result[lo_packet_stats.first] = lo_packet_stats.second;
    }
  }

  return result;
}

void PacketsStats::logAndUpdateStats() {
  static AllPacketTypesStats::PacketTypeStatsMap previous_received_packets_stats =
      received_packets_stats_.getStatsCopy();
  static AllPacketTypesStats::PacketTypeStatsMap previous_sent_packets_stats = sent_packets_stats_.getStatsCopy();

  auto getStatsJson = [](const AllPacketTypesStats::PacketTypeStatsMap& packets_stats_map) -> Json::Value {
    Json::Value ret;

    for (const auto& single_packet_stats : packets_stats_map) {
      Json::Value packet_json = single_packet_stats.second.getStatsJson(true);
      packet_json["type"] = single_packet_stats.first;
      ret.append(std::move(packet_json));
    }

    return ret;
  };

  auto tmp_received_packets_stats = received_packets_stats_.getStatsCopy();
  auto tmp_sent_packets_stats = received_packets_stats_.getStatsCopy();

  auto period_received_packets_stats = tmp_received_packets_stats - previous_received_packets_stats;
  auto period_sent_packets_stats = tmp_sent_packets_stats - previous_sent_packets_stats;

  LOG(log_nf_) << "Received packets stats: " << jsonToUnstyledString(getStatsJson(period_received_packets_stats));
  LOG(log_nf_) << "Sent packets stats: " << jsonToUnstyledString(getStatsJson(period_sent_packets_stats));

  received_packets_stats_.updatePeriodMaxStats(period_received_packets_stats);
  sent_packets_stats_.updatePeriodMaxStats(period_sent_packets_stats);

  previous_received_packets_stats = std::move(tmp_received_packets_stats);
  previous_sent_packets_stats = std::move(tmp_sent_packets_stats);
}

}  // namespace taraxa::network::tarcap