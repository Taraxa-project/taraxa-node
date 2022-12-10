#pragma once

#include <optional>

#include "network/tarcap/stats/max_stats.hpp"
#include "network/tarcap/stats/packets_stats.hpp"

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

  PacketStats getAllPacketsStatsCopy() const;
  Json::Value getAllPacketsMaxStats() const;
  Json::Value getStatsJson(bool include_duration_fields = true) const;

  /**
   * @brief Updates max stats
   * @note Partially thread-safe: all_packets_max_<...> class members are not protected through mutex
   */
  void updateAllPacketsMaxStats();

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

  // Max stats for all received packets combined from all peers
  // Note: all_packets_max_<...> class members are not protected by mutex
  MaxStats all_packets_max_stats_;
};

}  // namespace taraxa::network::tarcap