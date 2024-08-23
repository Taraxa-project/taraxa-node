#pragma once

#include <json/json.h>

#include "max_stats.hpp"
#include "packets_stats.hpp"

namespace taraxa::network::tarcap {

/**
 * @brief Stats for all packet types
 */
class PacketsStats {
 public:
  PacketsStats();
  PacketsStats(const PacketsStats &) = delete;
  PacketsStats &operator=(const PacketsStats &) = delete;
  PacketsStats(PacketsStats &&) = delete;
  PacketsStats &operator=(PacketsStats &&) = delete;

  ~PacketsStats() = default;

  using PerPacketStatsMap = std::unordered_map<std::string /*packet name*/, PacketStats>;

 public:
  void addPacket(const std::string &packet_type, const PacketStats &packet);

  std::pair<std::chrono::system_clock::time_point, PacketStats> getAllPacketsStatsCopy() const;
  Json::Value getStatsJson() const;

  /**
   * @brief Resets stats to zero
   */
  void resetStats();

 private:
  // Time point since which are the stats measured
  std::chrono::system_clock::time_point start_time_;

  // Stats for all packets types combined
  PacketStats all_packets_stats_;

  // Stas per individual packet type
  PerPacketStatsMap per_packet_stats_;
  mutable std::shared_mutex mutex_;
};

}  // namespace taraxa::network::tarcap