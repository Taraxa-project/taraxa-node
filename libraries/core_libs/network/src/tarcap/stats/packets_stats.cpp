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