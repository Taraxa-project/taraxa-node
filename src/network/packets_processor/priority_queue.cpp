#include "priority_queue.hpp"

namespace taraxa::network {

// TODO: set max_workers_count according to num of threads in threadpool
PriorityQueue::PriorityQueue()
    : packets_queues_({PacketsQueue(5) /* PacketPriority::High */, PacketsQueue(5) /* PacketPriority::Mid */,
                       PacketsQueue(3) /* PacketPriority::Low */}) {
  assert(packets_queues_.size() == PacketPriority::Count);
}

void PriorityQueue::pushBack(PacketData&& packet) { packets_queues_[packet.priority].pushBack(std::move(packet)); }

std::optional<PacketData> PriorityQueue::pop() {
  // Get first packet to be processed. Queues are ordered by priority
  // starting with highest priority and ending with lowest priority
  for (auto& queue : packets_queues_) {
    // Queue is not empty and max concurrent workers limit for queue not reached
    if (queue.isProcessingEligible()) {
      if (auto packet = queue.pop(blocked_packets_types_mask_.load()); packet.has_value()) {
        return packet;
      }

      // There was no unblocked packet to be processed in this specific queue
      continue;
    }
  }

  // There was no unblocked packet to be processed in all queues
  return {};
}

bool PriorityQueue::empty() const {
  for (const auto& queue : packets_queues_) {
    if (!queue.empty()) {
      return false;
    }
  }

  return true;
}

void PriorityQueue::markPacketAsBlocked(PacketType packet_type) {
  assert(!(blocked_packets_types_mask_ & packet_type));
  blocked_packets_types_mask_ |= packet_type;
}

void PriorityQueue::markPacketAsUnblocked(PacketType packet_type) {
  assert(blocked_packets_types_mask_ & packet_type);
  blocked_packets_types_mask_ ^= packet_type;
}

void PriorityQueue::updateDependenciesStart(const PacketData& packet) {
  packets_queues_[packet.priority].incrementActWorkersCount();

  // Process all dependencies here - it is called when packet processing has started
  // !!! Important - there is a "mirror" function updateDependenciesFinish and all dependencies that are set
  // here should be unset in updateDependenciesFinish

  // Packets that can be processed only 1 at the time
  //  _GetBlocksPacket -> serve syncing data to only 1 node at the time
  //  _BlocksPacket -> process sync dag blocks synchronously
  //  _PbftBlockPacket -> process sync pbft blocks synchronously
  if (packet.type == PacketType::_GetBlocksPacket || packet.type == PacketType::_BlocksPacket ||
      packet.type == PacketType::_PbftBlockPacket) {
    markPacketAsBlocked(packet.type);
  }
}

void PriorityQueue::updateDependenciesFinish(const PacketData& packet) {
  packets_queues_[packet.priority].decrementActWorkersCount();

  // Process all dependencies here - it is called when packet processing is finished

  if (packet.type == PacketType::_GetBlocksPacket || packet.type == PacketType::_BlocksPacket ||
      packet.type == PacketType::_PbftBlockPacket) {
    markPacketAsUnblocked(packet.type);
  }
}

}  // namespace taraxa::network