#pragma once

#include <libp2p/Common.h>

#include <chrono>
#include <optional>
#include <ostream>

namespace taraxa::network::tarcap {

struct PacketStats {
  dev::p2p::NodeID node_{0};
  uint64_t size_{0};
  bool is_unique_{false};
  std::chrono::microseconds total_duration_{0};
  friend std::ostream &operator<<(std::ostream &os, const PacketStats &stats);
};

class PacketsStats {
 public:
  struct PacketAvgStats {
    // Stats for all unique packets
    uint64_t total_count_{0};
    uint64_t total_size_{0};
    std::chrono::microseconds total_duration_{0};

    // Stats for unique packets
    uint64_t total_unique_count_{0};
    unsigned long total_unique_size_{0};
    std::chrono::microseconds total_unique_duration_{0};
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

}  // namespace taraxa::network::tarcap