#include "network/tarcap/stats/packets_stats.hpp"

#include "json/writer.h"

namespace taraxa::network::tarcap {

/**
 * @param value
 * @return unstyled json string (without new lines and whitespaces).
 */
std::string jsonToUnstyledString(const Json::Value& value) {
  Json::StreamWriterBuilder builder;
  builder["indentation"] = "";  // assume default for comments is None
  return Json::writeString(builder, value);
}

PacketsStats::PacketsStats(const addr_t& node_addr) { LOG_OBJECTS_CREATE("NETPER"); }

void PacketsStats::addReceivedPacket(const std::string& packet_type, const SinglePacketStats& packet) {
  received_packets_stats_.addPacket(packet_type, packet);

  auto packet_json = packet.getStatsJson();
  packet_json["type"] = packet_type;
  LOG(log_tr_) << "Received packet: " << jsonToUnstyledString(packet_json);
}

void PacketsStats::addSentPacket(const std::string& packet_type, const SinglePacketStats& packet) {
  sent_packets_stats_.addPacket(packet_type, packet);

  auto packet_json = packet.getStatsJson();
  packet_json["type"] = packet_type;
  LOG(log_tr_) << "Sent packet: " << jsonToUnstyledString(packet_json);
}

const AllPacketTypesStats& PacketsStats::getSentPacketsStats() const { return sent_packets_stats_; }

const AllPacketTypesStats& PacketsStats::getReceivedPacketsStats() const { return received_packets_stats_; }

void PacketsStats::logAndUpdateStats() {
  static AllPacketTypesStats previous_received_packets_stats = received_packets_stats_;
  static AllPacketTypesStats previous_sent_packets_stats = sent_packets_stats_;

  auto period_received_packets_stats = received_packets_stats_ - previous_received_packets_stats;
  auto period_sent_packets_stats = sent_packets_stats_ - previous_sent_packets_stats;

  LOG(log_nf_) << "Received packets stats: " << jsonToUnstyledString(period_received_packets_stats.getStatsJson());
  LOG(log_nf_) << "Sent packets stats: " << jsonToUnstyledString(period_sent_packets_stats.getStatsJson());

  received_packets_stats_.updatePeriodMaxStats(period_received_packets_stats);
  sent_packets_stats_.updatePeriodMaxStats(period_sent_packets_stats);

  previous_received_packets_stats = received_packets_stats_;
  previous_sent_packets_stats = sent_packets_stats_;
}

}  // namespace taraxa::network::tarcap