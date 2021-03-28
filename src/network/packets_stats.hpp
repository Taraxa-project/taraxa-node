#pragma once

#include <libp2p/Common.h>

#include <optional>
#include <ostream>

#include "packet_debug_info.hpp"
#include "util/timer.hpp"

namespace taraxa {

class PacketStats {
 public:
  PacketStats() = default;
  PacketStats(const dev::p2p::NodeID &nodeID, uint64_t size);

  void setDebugInfo(std::unique_ptr<PacketDebugInfo> &&dbg_info);
  std::unique_ptr<PacketDebugInfo> &getDebugInfo();
  void setUnique(bool flag = true);

  void restartTimer();
  void stopTimer();

  template <typename DurationType = std::chrono::microseconds>
  uint64_t getActualProcessingDuration() {
    return taraxa::stopTimer<DurationType>(processing_start_time_);
  }

 private:
  dev::p2p::NodeID node_;
  uint64_t size_;
  bool is_unique_;
  uint64_t total_duration_;  // [us]
  std::unique_ptr<PacketDebugInfo> debug_info_;
  std::chrono::steady_clock::time_point processing_start_time_;

  friend std::ostream &operator<<(std::ostream &os, const PacketStats &stats);
  friend class PacketsStats;
};

class PacketsStats {
 public:
  struct PacketAvgStats {
    // Stats for all unique packets
    uint64_t total_count_{0};
    uint64_t total_size_{0};
    uint64_t total_duration_{0};  // [us]

    // Stats for unique packets
    uint64_t total_unique_count_{0};
    uint64_t total_unique_size_{0};
    uint64_t total_unique_duration_{0};  // [us]
    friend std::ostream &operator<<(std::ostream &os, const PacketAvgStats &stats);

    PacketAvgStats operator-(const PacketAvgStats &ro) const;
  };

 public:
  PacketsStats() = default;
  PacketsStats(const PacketsStats &ro);
  PacketsStats &operator=(const PacketsStats &ro);
  PacketsStats operator-(const PacketsStats &ro) const;

  void addPacket(const std::string &packet_type, const PacketStats &packet);
  std::optional<PacketAvgStats> getPacketStats(const std::string &packet_type) const;

  friend std::ostream &operator<<(std::ostream &os, const PacketsStats &packets_stats);

 private:
  std::unordered_map<std::string /*packet name*/, PacketAvgStats> stats_;
  std::mutex mutex_;
};

}  // namespace taraxa