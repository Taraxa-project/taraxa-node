#include "packets_stats.hpp"

namespace taraxa::network::tarcap {

PacketsStats::PacketsStats(const addr_t& node_addr) { LOG_OBJECTS_CREATE("NETPER"); }

void PacketsStats::addReceivedPacket(const dev::p2p::NodeID& node, const std::string& packet_type,
                                     const SinglePacketStats& packet) {
  received_packets_stats_.addPacket(packet_type, packet);

  LOG(log_dg_) << "(\"" << node << "\") received " << packet_type << " packet from (\"" << packet.node_
               << "\"). Stats: " << packet;
}

void PacketsStats::addSentPacket(const dev::p2p::NodeID& node, const std::string& packet_type,
                                 const SinglePacketStats& packet) {
  sent_packets_stats_.addPacket(packet_type, packet);

  LOG(log_dg_) << "(\"" << node << "\") sent " << packet_type << " packet to (\"" << packet.node_
               << "\"). Stats: " << packet;
}

const PacketsAvgStats& PacketsStats::getSentPacketsStats() const { return sent_packets_stats_; }

const PacketsAvgStats& PacketsStats::getReceivedPacketsStats() const { return received_packets_stats_; }

void PacketsStats::logStats() {
  static PacketsAvgStats previous_received_packets_stats = received_packets_stats_;
  static PacketsAvgStats previous_sent_packets_stats = sent_packets_stats_;

  LOG(log_nf_) << "Received packets stats: " << received_packets_stats_ - previous_received_packets_stats;
  LOG(log_nf_) << "Sent packets stats: " << sent_packets_stats_ - previous_sent_packets_stats;

  previous_received_packets_stats = received_packets_stats_;
  previous_sent_packets_stats = sent_packets_stats_;
}

}  // namespace taraxa::network::tarcap