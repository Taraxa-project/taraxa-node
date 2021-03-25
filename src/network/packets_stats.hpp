#pragma once

#include <libp2p/Common.h>

#include <chrono>
#include <optional>
#include <ostream>

namespace taraxa {

struct PacketStats {
  dev::p2p::NodeID node_{0};
  size_t size_{0};
  bool is_unique_{false};
  std::chrono::microseconds total_duration_{0};
  friend std::ostream &operator<<(std::ostream &os, const PacketStats &stats);
};

class PacketsStats {
 public:
  struct PacketAvgStats {
    // Stats for all unique packets
    size_t total_count_{0};
    unsigned long total_size_{0};
    std::chrono::microseconds total_duration_{0};

    // Stats for unique packets
    size_t total_unique_count_{0};
    unsigned long total_unique_size_{0};
    std::chrono::microseconds total_unique_duration_{0};
    friend std::ostream &operator<<(std::ostream &os, const PacketAvgStats &stats);
  };

 public:
  PacketsStats() = default;

  void addPacket(const std::string &packet_type, const PacketStats &packet);
  std::optional<const PacketAvgStats> getPacketStats(const std::string &packet_type) const;
  void clearData();

  friend std::ostream &operator<<(std::ostream &os, const PacketsStats &packets_stats);

 private:
  std::unordered_map<std::string /*packet name*/, PacketAvgStats> stats_;
  std::mutex mutex_;
};

}  // namespace taraxa