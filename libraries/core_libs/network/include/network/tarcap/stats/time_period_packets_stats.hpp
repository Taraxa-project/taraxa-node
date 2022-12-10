#pragma once

#include "common/types.hpp"
#include "logger/logger.hpp"
#include "network/tarcap/stats/max_stats.hpp"
#include "network/tarcap/stats/packets_stats.hpp"

namespace taraxa::network::tarcap {

class PeersState;

/**
 * @brief Stats for all received and sent packets of all types
 */
class TimePeriodPacketsStats {
 public:
  TimePeriodPacketsStats(const addr_t& node_addr = {});

  void addReceivedPacket(const std::string& packet_type, const dev::p2p::NodeID& node, const PacketStats& packet);
  void addSentPacket(const std::string& packet_type, const dev::p2p::NodeID& node, const PacketStats& packet);

  /**
   * @brief Logs both received as well as sent packets stats + updates max count/size and reset stats
   */
  void processStats(const std::shared_ptr<PeersState>& peers_state);

 private:
  PacketsStats sent_packets_stats_;
  PacketsStats received_packets_stats_;

  // Max stats for all received packets combined per peer
  MaxStats peer_max_stats_;

  // Declare logger instances
  LOG_OBJECTS_DEFINE
};

}  // namespace taraxa::network::tarcap
