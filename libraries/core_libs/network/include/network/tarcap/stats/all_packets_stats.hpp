#pragma once

#include "common/types.hpp"
#include "logger/logger.hpp"
#include "packets_stats.hpp"

namespace taraxa::network::tarcap {

/**
 * @brief Stats for all received and sent packets of all types
 */
class AllPacketsStats {
 public:
  AllPacketsStats(const addr_t& node_addr = {});

  void addReceivedPacket(const std::string& packet_type, const dev::p2p::NodeID& node, const PacketStats& packet);
  void addSentPacket(const std::string& packet_type, const dev::p2p::NodeID& node, const PacketStats& packet);

  const PacketsStats& getSentPacketsStats() const;
  const PacketsStats& getReceivedPacketsStats() const;

  /**
   * @brief Logs both received as well as sent avg packets stats + updates max count/size stats per period
   */
  void logAndUpdateStats();

 private:
  PacketsStats sent_packets_stats_;
  PacketsStats received_packets_stats_;

  // Declare logger instances
  LOG_OBJECTS_DEFINE
};

}  // namespace taraxa::network::tarcap
