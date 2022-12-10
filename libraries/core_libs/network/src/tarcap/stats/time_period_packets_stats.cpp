#include "network/tarcap/stats/time_period_packets_stats.hpp"

#include "common/util.hpp"
#include "json/writer.h"
#include "network/tarcap/shared_states/peers_state.hpp"

namespace taraxa::network::tarcap {

TimePeriodPacketsStats::TimePeriodPacketsStats(const addr_t& node_addr) { LOG_OBJECTS_CREATE("NETPER"); }

void TimePeriodPacketsStats::addReceivedPacket(const std::string& packet_type, const dev::p2p::NodeID& node,
                                               const PacketStats& packet) {
  received_packets_stats_.addPacket(packet_type, packet);
  LOG(log_tr_) << "Received packet: " << packet.getStatsJsonStr(packet_type, node);
}

void TimePeriodPacketsStats::addSentPacket(const std::string& packet_type, const dev::p2p::NodeID& node,
                                           const PacketStats& packet) {
  sent_packets_stats_.addPacket(packet_type, packet);
  LOG(log_tr_) << "Sent packet: " << packet.getStatsJsonStr(packet_type, node);
}

void TimePeriodPacketsStats::processStats(const std::shared_ptr<PeersState>& peers_state) {
  LOG(log_nf_) << "Received packets stats: " << jsonToUnstyledString(received_packets_stats_.getStatsJson(true));
  LOG(log_nf_) << "Sent packets stats: " << jsonToUnstyledString(sent_packets_stats_.getStatsJson(true));

  // Update max stats for all received packets from all peers - must be called before resetStats
  received_packets_stats_.updateAllPacketsMaxStats();

  // Reset stats for both received & sent packets stats from all peers
  received_packets_stats_.resetStats();
  sent_packets_stats_.resetStats();

  // Update max packets stats per peer
  for (const auto& peer : peers_state->getAllPeers()) {
    peer_max_stats_.updateMaxStats(peer.second->getAllPacketsStatsCopy());

    // Reset packet stats per peer
    peer.second->resetPacketsStats();
  }

  // Log max stats
  Json::Value max_stats_json;
  max_stats_json["all_max_stats"] = received_packets_stats_.getAllPacketsMaxStats();
  max_stats_json["peer_max_stats"] = peer_max_stats_.getMaxStatsJson();
  LOG(log_dg_) << "Max packets stats: " << jsonToUnstyledString(max_stats_json);
}

}  // namespace taraxa::network::tarcap