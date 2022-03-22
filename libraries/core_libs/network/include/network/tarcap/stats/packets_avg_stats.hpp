#pragma once

#include <libp2p/Common.h>

#include <chrono>
#include <optional>
#include <ostream>

#include "json/value.h"

namespace taraxa::network::tarcap {

/**
 * @brief Stats for single received (or sent) packet
 */
struct SinglePacketStats {
  dev::p2p::NodeID node_{0};
  uint64_t size_{0};
  // How long did actual packet processing took
  std::chrono::microseconds processing_duration_{0};
  // How long waiting in threadpool until it was processed took
  std::chrono::microseconds tp_wait_duration_{0};

  Json::Value getStatsJson(bool include_duration_fields = true) const;
  std::string getStatsJsonStr(const std::string &packet_type, bool include_duration_fields = true) const;
};

/**
 * @brief Stats for all received (or sent) packets of all types
 */
class AllPacketTypesStats {
 public:
  /**
   * @brief Stats for all received (or sent) packets of one type
   */
  struct PacketTypeStats {
    uint64_t count_{0};
    uint64_t size_{0};
    std::chrono::microseconds processing_duration_{0};
    std::chrono::microseconds tp_wait_duration_{0};

    Json::Value getStatsJson(bool include_duration_fields = true) const;
    PacketTypeStats operator-(const PacketTypeStats &ro) const;
  };

  using PacketTypeStatsMap = std::unordered_map<std::string /*packet name*/, PacketTypeStats>;

 public:
  AllPacketTypesStats() = default;
  ~AllPacketTypesStats() = default;

  AllPacketTypesStats(const AllPacketTypesStats &ro) = delete;
  AllPacketTypesStats &operator=(const AllPacketTypesStats &ro) = delete;
  AllPacketTypesStats(AllPacketTypesStats &&ro) = delete;
  AllPacketTypesStats &operator=(AllPacketTypesStats &&ro) = delete;

  void addPacket(const std::string &packet_type, const SinglePacketStats &packet);

  PacketTypeStatsMap getStatsCopy() const;
  Json::Value getStatsJson(bool include_duration_fields = true) const;

  /**
   * @brief Updates max_counts/sizes_stats based on period_stats if needed
   * @param period_stats packets stats during certain time period
   */
  void updatePeriodMaxStats(const AllPacketTypesStats::PacketTypeStatsMap &period_stats);
  Json::Value getPeriodMaxStatsJson(bool include_duration_fields = true) const;

 private:
  PacketTypeStatsMap stats_;
  mutable std::shared_mutex mutex_;

  // Statistics about max number of packets (of the same type) received during fixed time period
  std::unordered_map<std::string /*packet name*/, PacketTypeStats> max_counts_stats_;
  // Statistics about max size of packets (of the same type) received during fixed time period
  std::unordered_map<std::string /*packet name*/, PacketTypeStats> max_sizes_stats_;
  mutable std::shared_mutex max_stats_mutex_;
};

}  // namespace taraxa::network::tarcap