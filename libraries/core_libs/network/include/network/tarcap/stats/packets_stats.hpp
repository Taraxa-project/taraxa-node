#pragma once

#include <optional>

#include "packet_stats.hpp"

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

  // TODO: remove copy functions
  PacketStats getAllPacketsStatsCopy() const;
  PerPacketStatsMap getPerPacketStatsCopy() const;
  Json::Value getStatsJson(bool include_duration_fields = true) const;

  /**
   * @brief Updates max_counts/sizes_stats based on period_stats if needed
   * @param period_stats packets stats during certain time period
   */
  void updatePeriodMaxStats(const PacketsStats::PerPacketStatsMap &period_stats);
  Json::Value getPeriodMaxStatsJson(bool include_duration_fields = true) const;

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

  // TODO: maybe put max stats into separate class together with max stats per peer ?
  // Statistics about max number of packets (of the same type) received during fixed time period
  std::unordered_map<std::string /*packet name*/, PacketStats> max_counts_stats_;
  // Statistics about max size of packets (of the same type) received during fixed time period
  std::unordered_map<std::string /*packet name*/, PacketStats> max_sizes_stats_;
  mutable std::shared_mutex max_stats_mutex_;
};

PacketsStats::PerPacketStatsMap operator-(const PacketsStats::PerPacketStatsMap &lo,
                                          const PacketsStats::PerPacketStatsMap &ro);

}  // namespace taraxa::network::tarcap