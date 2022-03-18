#pragma once

#include "common/types.hpp"
#include "logger/logger.hpp"
#include "packets_avg_stats.hpp"

namespace taraxa::network::tarcap {

/**
 * @brief Stats for all received and sent packets of all types
 */
class PacketsStats {
 public:
  PacketsStats(const addr_t& node_addr = {});

  void addReceivedPacket(const std::string& packet_type, const SinglePacketStats& packet);
  void addSentPacket(const std::string& packet_type, const SinglePacketStats& packet);

  const AllPacketTypesStats& getSentPacketsStats() const;
  const AllPacketTypesStats& getReceivedPacketsStats() const;

  /**
   * @brief Logs both received as well as sent avg packets stats + updates max count/size stats per period
   */
  void logAndUpdateStats();

 private:
  AllPacketTypesStats sent_packets_stats_;
  AllPacketTypesStats received_packets_stats_;

  // Declare logger instances
  LOG_OBJECTS_DEFINE
};

}  // namespace taraxa::network::tarcap
