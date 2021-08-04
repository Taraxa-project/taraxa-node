#pragma once

#include <libp2p/Common.h>

#include <chrono>
#include <optional>
#include <ostream>

namespace taraxa::network::tarcap {

/**
 * @brief Stats for single received (or sent) packet
 */
struct SinglePacketStats {
  dev::p2p::NodeID node_{0};
  uint64_t size_{0};
  bool is_unique_{false};
  // How long did actual packet processing took
  std::chrono::microseconds processing_duration_{0};
  // How long waiting in threadpool until it was processed took
  std::chrono::microseconds tp_wait_duration_{0};
  friend std::ostream &operator<<(std::ostream &os, const SinglePacketStats &stats);
};

/**
 * @brief Average stats for all received (or sent) packets of all types
 */
class PacketsAvgStats {
 public:
  /**
   * @brief Average stats for all received (or sent) packets of one type
   */
  struct SinglePacketAvgStats {
    // Stats for all unique packets
    uint64_t total_count_{0};
    uint64_t total_size_{0};
    std::chrono::microseconds processing_duration_{0};
    std::chrono::microseconds tp_wait_duration_{0};

    // Stats for unique packets
    uint64_t total_unique_count_{0};
    unsigned long total_unique_size_{0};
    std::chrono::microseconds processing_unique_duration_{0};
    std::chrono::microseconds tp_wait_unique_duration_{0};
    friend std::ostream &operator<<(std::ostream &os, const SinglePacketAvgStats &stats);

    SinglePacketAvgStats operator-(const SinglePacketAvgStats &ro) const;
  };

 public:
  PacketsAvgStats() = default;
  PacketsAvgStats(const PacketsAvgStats &ro);
  PacketsAvgStats &operator=(const PacketsAvgStats &ro);
  PacketsAvgStats operator-(const PacketsAvgStats &ro) const;

  void addPacket(const std::string &packet_type, const SinglePacketStats &packet);
  std::optional<SinglePacketAvgStats> getPacketStats(const std::string &packet_type) const;

  friend std::ostream &operator<<(std::ostream &os, const PacketsAvgStats &packets_stats);

 private:
  std::unordered_map<std::string /*packet name*/, SinglePacketAvgStats> stats_;
  std::mutex mutex_;
};

}  // namespace taraxa::network::tarcap