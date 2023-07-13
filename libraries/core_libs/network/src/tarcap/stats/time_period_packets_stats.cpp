#include "network/tarcap/stats/time_period_packets_stats.hpp"

#include "common/util.hpp"
#include "json/writer.h"
#include "network/tarcap/shared_states/peers_state.hpp"

namespace taraxa::network::tarcap {

TimePeriodPacketsStats::TimePeriodPacketsStats(std::chrono::milliseconds reset_time_period, const addr_t& node_addr)
    : kResetTimePeriod(reset_time_period) {
  LOG_OBJECTS_CREATE("NETPER");
}

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

std::pair<bool, std::chrono::milliseconds> TimePeriodPacketsStats::validMaxStatsTimePeriod(
    const std::chrono::system_clock::time_point& start_time) const {
  using namespace std::chrono_literals;

  const auto reset_time_period =
      std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - start_time);

  // Let the time period be max 500ms off to both sides
  if (reset_time_period >= kResetTimePeriod - 500ms && reset_time_period <= kResetTimePeriod + 500ms) {
    return {true, reset_time_period};
  }

  return {false, reset_time_period};
}

void TimePeriodPacketsStats::processStats(const std::vector<std::shared_ptr<TaraxaPeer>>& all_peers) {
  LOG(log_nf_) << "Received packets stats: " << jsonToUnstyledString(received_packets_stats_.getStatsJson());
  LOG(log_nf_) << "Sent packets stats: " << jsonToUnstyledString(sent_packets_stats_.getStatsJson());

  // Reset stats for both received & sent packets stats from all peers
  received_packets_stats_.resetStats();
  sent_packets_stats_.resetStats();

  MaxStats peer_max_stats_per_time_window;

  for (const auto& peer : all_peers) {
    const auto [start_time, peer_packets_stats] = peer->getAllPacketsStatsCopy();

    // Check if processStats was called on expected time period so we can use current stats for max stats
    if (const auto valid_reset_period = validMaxStatsTimePeriod(start_time); !valid_reset_period.first) {
      LOG(log_wr_) << "Cannot process stats from peer " << peer->getId() << " for \"max\" stats. Current reset period "
                   << valid_reset_period.second.count() << ", expected reset period " << kResetTimePeriod.count();
      continue;
    }

    // Update max packets stats per peer
    peer_max_stats_.updateMaxStats(peer_packets_stats);
    peer_max_stats_per_time_window.updateMaxStats(peer_packets_stats);
  }

  // Log max stats
  Json::Value max_stats_json;
  max_stats_json["time_period_ms"] = kResetTimePeriod.count();
  max_stats_json["peer_overall_max_stats"] = peer_max_stats_.getMaxStatsJson();
  max_stats_json["peer_current_max_stats"] = peer_max_stats_per_time_window.getMaxStatsJson();
  LOG(log_dg_) << "Max packets stats: " << jsonToUnstyledString(max_stats_json);
}

}  // namespace taraxa::network::tarcap