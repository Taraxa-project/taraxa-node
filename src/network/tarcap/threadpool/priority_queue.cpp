#include "priority_queue.hpp"

namespace taraxa::network::tarcap {

PriorityQueue::PriorityQueue(size_t tp_workers_count) {
  assert(packets_queues_.size() == PacketData::PacketPriority::Count);

  // high priority packets(consensus - votes) max concurrent workers - 40% of tp_workers_count
  // mid priority packets(dag/pbft blocks, txs) max concurrent workers - 50% of tp_workers_count
  // low priority packets(syncing, status, ...) max concurrent workers - 30% of tp_workers_count
  size_t high_priority_queue_workers = tp_workers_count * 4 / 10;
  size_t mid_priority_queue_workers = tp_workers_count * 5 / 10;
  size_t low_priority_queue_workers = tp_workers_count - (high_priority_queue_workers + mid_priority_queue_workers);

  packets_queues_[PacketData::PacketPriority::High].setMaxWorkersCount(high_priority_queue_workers);
  packets_queues_[PacketData::PacketPriority::Mid].setMaxWorkersCount(mid_priority_queue_workers);
  packets_queues_[PacketData::PacketPriority::Low].setMaxWorkersCount(low_priority_queue_workers);
}

void PriorityQueue::pushBack(PacketData&& packet) { packets_queues_[packet.priority_].pushBack(std::move(packet)); }

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

void PriorityQueue::markPacketAsBlocked(SubprotocolPacketType packet_type) {
  assert(!(blocked_packets_types_mask_ & packet_type));
  blocked_packets_types_mask_ |= packet_type;
}

void PriorityQueue::markPacketAsUnblocked(SubprotocolPacketType packet_type) {
  assert(blocked_packets_types_mask_ & packet_type);
  blocked_packets_types_mask_ ^= packet_type;
}

void PriorityQueue::updateDependenciesStart(const PacketData& packet) {
  packets_queues_[packet.priority_].incrementActWorkersCount();

  // Process all dependencies here - it is called when packet processing has started
  // !!! Important - there is a "mirror" function updateDependenciesFinish and all dependencies that are set
  // here should be unset in updateDependenciesFinish

  // Packets that can be processed only 1 at the time
  //  _GetBlocksPacket -> serve syncing data to only 1 node at the time
  //  _BlocksPacket -> process sync dag blocks synchronously
  //  _PbftBlockPacket -> process sync pbft blocks synchronously
  if (packet.type_ == SubprotocolPacketType::GetBlocksPacket || packet.type_ == SubprotocolPacketType::BlocksPacket ||
      packet.type_ == SubprotocolPacketType::PbftBlockPacket) {
    markPacketAsBlocked(packet.type_);
  }

  // TODO: when processing transactionPacket, mark the index (or time it was received) and block processing of all
  //       dag block packets that were received after that. No need to block processing of dag blocks packets received
  //       before as it should not be possible to send dag block before sending txs it contains... This way it should be
  //       fairly efficient
}

void PriorityQueue::updateDependenciesFinish(const PacketData& packet) {
  packets_queues_[packet.priority_].decrementActWorkersCount();

  // Process all dependencies here - it is called when packet processing is finished

  if (packet.type_ == SubprotocolPacketType::GetBlocksPacket || packet.type_ == SubprotocolPacketType::BlocksPacket ||
      packet.type_ == SubprotocolPacketType::PbftBlockPacket) {
    markPacketAsUnblocked(packet.type_);
  }
}

}  // namespace taraxa::network::tarcap