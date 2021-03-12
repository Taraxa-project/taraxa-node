#pragma once

#include <libp2p/Common.h>

#include <chrono>
#include <ostream>

#include "packet_type.hpp"

namespace taraxa {

struct PacketStats {
  dev::p2p::NodeID node_{0};
  std::chrono::system_clock::time_point time_;
  size_t size_{0};
  std::chrono::milliseconds total_duration_{0};  // [ms]
  friend std::ostream &operator<<(std::ostream &os, const PacketStats &stats);
};

class PacketsStats {
 public:
  struct PacketAvgStats {
    size_t total_count_{0};
    unsigned long total_size_{0};
    std::chrono::milliseconds total_duration_{0};

    void clearData();
    friend std::ostream &operator<<(std::ostream &os, const PacketAvgStats &stats);
  };

  using PacketType = unsigned;

 public:
  PacketsStats(SubprotocolPacketType packetsCount = SubprotocolPacketType::PacketCount);

  void addPacket(PacketType packet_type, const PacketStats &packet);
  void clearData();
  friend std::ostream &operator<<(std::ostream &os, const PacketsStats &packets_stats);

 private:
  std::unordered_map<PacketType, PacketAvgStats> stats_;
  SubprotocolPacketType packets_count_;
};

}  // namespace taraxa
