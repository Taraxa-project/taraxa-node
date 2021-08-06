#pragma once

#include <libdevcore/RLP.h>

#include <array>
#include <utility>

#include "network/tarcap/packet_types.hpp"
#include "packets_queue.hpp"
#include "logger/log.hpp"

namespace taraxa::network::tarcap {

class PriorityQueue {
 public:
  PriorityQueue(size_t tp_workers_count, const addr_t& node_addr = {});

  void pushBack(PacketData&& packet);

  /**
   * @return std::optional<PacketData> packet with the highest priority & oldest "receive" time
   */
  std::optional<PacketData> pop();

  bool empty() const;

  void updateDependenciesStart(const PacketData& packet);

  void updateDependenciesFinish(const PacketData& packet);

 private:
  void markPacketAsBlocked(PriorityQueuePacketType packet_type);
  void markPacketAsUnblocked(PriorityQueuePacketType packet_type);

 private:
  // Queues that group packets by it's priority.
  // All packets with PacketPriority::High go to packets_queues_[PacketPriority::High], etc...
  std::array<PacketsQueue, PacketData::PacketPriority::Count> packets_queues_{PacketsQueue(), PacketsQueue(),
                                                                              PacketsQueue()};

  // Mask with all packets types that are currently blocked for processing in another threads due to dependencies, e.g.
  // syncing packets must be processed synchronously one by one, etc...
  // blocked_packets_types_mask_ OR QueuePacketType -> sets QueuePacketType as blocked packet
  // blocked_packets_types_mask_ XOR QueuePacketType -> sets QueuePacketType as unblocked packet (note: packet must be
  // marked as blocked - 1 for XOR to work correctly)
  std::atomic<uint32_t> blocked_packets_types_mask_;

  // How many workers can process packets from all the queues at the same time
  size_t MAX_TOTAL_WORKERS_COUNT;

  // How many workers are currently processing packets from all the queues at the same time
  std::atomic<size_t> act_total_workers_count_;

  // Declare logger instances
  LOG_OBJECTS_DEFINE
};

}  // namespace taraxa::network::tarcap
