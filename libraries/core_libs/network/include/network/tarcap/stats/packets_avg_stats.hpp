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

  friend std::ostream &operator<<(std::ostream &os, const SinglePacketStats &stats);
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

    PacketTypeStats operator-(const PacketTypeStats &ro) const;
    friend std::ostream &operator<<(std::ostream &os, const PacketTypeStats &stats);
  };

 public:
  AllPacketTypesStats() = default;
  AllPacketTypesStats(const AllPacketTypesStats &ro);
  AllPacketTypesStats &operator=(const AllPacketTypesStats &ro);
  AllPacketTypesStats operator-(const AllPacketTypesStats &ro) const;

  void addPacket(const std::string &packet_type, const SinglePacketStats &packet);
  std::optional<PacketTypeStats> getPacketTypeStats(const std::string &packet_type) const;

  std::unordered_map<std::string, PacketTypeStats> getStatsCopy() const;
  Json::Value getStatsJson(bool include_duration_fields) const;

 private:
  std::unordered_map<std::string /*packet name*/, PacketTypeStats> stats_;
  mutable std::shared_mutex mutex_;
};

std::ostream &operator<<(std::ostream &os, const AllPacketTypesStats &packets_stats);

}  // namespace taraxa::network::tarcap