#include "network/tarcap/stats/all_packets_stats.hpp"

#include "common/util.hpp"
#include "json/writer.h"

namespace taraxa::network::tarcap {

AllPacketsStats::AllPacketsStats(const addr_t& node_addr) { LOG_OBJECTS_CREATE("NETPER"); }

void AllPacketsStats::addReceivedPacket(const std::string& packet_type, const dev::p2p::NodeID& node,
                                        const PacketStats& packet) {
  received_packets_stats_.addPacket(packet_type, packet);
  LOG(log_tr_) << "Received packet: " << packet.getStatsJsonStr(packet_type, node);
}

void AllPacketsStats::addSentPacket(const std::string& packet_type, const dev::p2p::NodeID& node,
                                    const PacketStats& packet) {
  sent_packets_stats_.addPacket(packet_type, packet);
  LOG(log_tr_) << "Sent packet: " << packet.getStatsJsonStr(packet_type, node);
}

const PacketsStats& AllPacketsStats::getSentPacketsStats() const { return sent_packets_stats_; }

const PacketsStats& AllPacketsStats::getReceivedPacketsStats() const { return received_packets_stats_; }

void AllPacketsStats::logAndUpdateStats() {
  static PacketsStats::PerPacketStatsMap previous_received_packets_stats =
      received_packets_stats_.getPerPacketStatsCopy();
  static PacketsStats::PerPacketStatsMap previous_sent_packets_stats = sent_packets_stats_.getPerPacketStatsCopy();

  // TODO: reuse getStatsJson from AllPacketsStats
  auto getStatsJson = [](const PacketsStats::PerPacketStatsMap& packets_stats_map) -> Json::Value {
    Json::Value ret;

    for (const auto& single_packet_stats : packets_stats_map) {
      Json::Value packet_json = single_packet_stats.second.getStatsJson(true);
      packet_json["type"] = single_packet_stats.first;
      ret.append(std::move(packet_json));
    }

    return ret;
  };

  // TODO: do not use substraction but rather reseting...
  auto tmp_received_packets_stats = received_packets_stats_.getPerPacketStatsCopy();
  auto tmp_sent_packets_stats = sent_packets_stats_.getPerPacketStatsCopy();

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