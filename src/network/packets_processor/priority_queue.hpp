#pragma once

#include <libdevcore/RLP.h>

#include <utility>

#include "network/packet_types.hpp"
#include "network/packets_processor/packets_queue.hpp"

namespace taraxa::network {

class PriorityQueue {
 public:
  PriorityQueue();

  void pushBack(PacketData&& packet);

  /**
   * @return std::optional<PacketData> packet with the highest priority & oldest "receive" time
   */
  std::optional<PacketData> pop();

  bool empty() const;

  void updateDependenciesStart(const PacketData& packet);

  void updateDependenciesFinish(const PacketData& packet);

 private:
  void markPacketAsBlocked(PacketType packet_type);
  void markPacketAsUnblocked(PacketType packet_type);

 private:
  // Queues that group packets by it's priority.
  // All packets with PacketPriority::High go to packets_queues_[PacketPriority::High], etc...
  std::vector<PacketsQueue> packets_queues_;

  // Mask with all packets types that are currently blocked for processing in another threads due to dependencies, e.g.
  // syncing packets must be processed synchronously one by one, etc...
  // blocked_packets_types_mask_ OR QueuePacketType -> sets QueuePacketType as blocked packet
  // blocked_packets_types_mask_ XOR QueuePacketType -> sets QueuePacketType as unblocked packet (note: packet must be
  // marked as blocked - 1 for XOR to work correctly)
  std::atomic<uint32_t> blocked_packets_types_mask_;
};

}  // namespace taraxa::network
