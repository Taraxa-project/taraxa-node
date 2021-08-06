#include "priority_queue.hpp"

namespace taraxa::network::tarcap {

PriorityQueue::PriorityQueue(size_t tp_workers_count, const addr_t& node_addr) :
    MAX_TOTAL_WORKERS_COUNT(tp_workers_count), act_total_workers_count_(0) {
  assert(packets_queues_.size() == PacketData::PacketPriority::Count);
  assert(tp_workers_count >= 1);

  LOG_OBJECTS_CREATE("PRIORITY_QUEUE");

  // high priority packets(consensus - votes) max concurrent workers - 40% of tp_workers_count
  // mid priority packets(dag/pbft blocks, txs) max concurrent workers - 50% of tp_workers_count
  // low priority packets(syncing, status, ...) max concurrent workers - 30% of tp_workers_count
  size_t high_priority_queue_workers = std::max(1, static_cast<int>(MAX_TOTAL_WORKERS_COUNT * 4 / 10));
  size_t mid_priority_queue_workers = std::max(1, static_cast<int>(MAX_TOTAL_WORKERS_COUNT * 5 / 10));
  size_t low_priority_queue_workers = std::max(1, static_cast<int>(MAX_TOTAL_WORKERS_COUNT - (high_priority_queue_workers + mid_priority_queue_workers)));

  packets_queues_[PacketData::PacketPriority::High].setMaxWorkersCount(high_priority_queue_workers);
  packets_queues_[PacketData::PacketPriority::Mid].setMaxWorkersCount(mid_priority_queue_workers);
  packets_queues_[PacketData::PacketPriority::Low].setMaxWorkersCount(low_priority_queue_workers);

  LOG(log_nf_) << "Priority queue initialized accordingly: "
               << "total num of workers = " << MAX_TOTAL_WORKERS_COUNT
               << ", High priority packets queue = " << high_priority_queue_workers
               << ", Mid priority packets queue = " << mid_priority_queue_workers
               << ", Low priority packets queue = " << low_priority_queue_workers;
}

void PriorityQueue::pushBack(PacketData&& packet) { packets_queues_[packet.priority_].pushBack(std::move(packet)); }

std::optional<PacketData> PriorityQueue::pop() {
  if (act_total_workers_count_ >= MAX_TOTAL_WORKERS_COUNT) {
    LOG(log_tr_) << "MAX_TOTAL_WORKERS_COUNT(" << MAX_TOTAL_WORKERS_COUNT << ") reached, unable to pop data.";
    return {};
  }

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
  LOG(log_tr_) << "No non-blocked packets to be processed.";
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

void PriorityQueue::markPacketAsBlocked(PriorityQueuePacketType packet_type) {
  assert(!(blocked_packets_types_mask_ & packet_type));
  blocked_packets_types_mask_ |= packet_type;
}

void PriorityQueue::markPacketAsUnblocked(PriorityQueuePacketType packet_type) {
  assert(blocked_packets_types_mask_ & packet_type);
  blocked_packets_types_mask_ ^= packet_type;
}

void PriorityQueue::updateDependenciesStart(const PacketData& packet) {
  assert(act_total_workers_count_ < MAX_TOTAL_WORKERS_COUNT);
  act_total_workers_count_++;

  packets_queues_[packet.priority_].incrementActWorkersCount();

  // Process all dependencies here - it is called when packet processing has started
  // !!! Important - there is a "mirror" function updateDependenciesFinish and all dependencies that are set
  // here should be unset in updateDependenciesFinish

  // Packets that can be processed only 1 at the time
  //  _GetBlocksPacket -> serve syncing data to only 1 node at the time
  //  _BlocksPacket -> process sync dag blocks synchronously
  //  _PbftBlockPacket -> process sync pbft blocks synchronously
  if (packet.type_ == PriorityQueuePacketType::PQ_GetBlocksPacket || packet.type_ == PriorityQueuePacketType::PQ_BlocksPacket ||
      packet.type_ == PriorityQueuePacketType::PQ_PbftBlockPacket) {
    markPacketAsBlocked(packet.type_);
  }

  // TODO: when processing transactionPacket, mark the index (or time it was received) and block processing of all
  //       dag block packets that were received after that. No need to block processing of dag blocks packets received
  //       before as it should not be possible to send dag block before sending txs it contains... This way it should be
  //       fairly efficient
}

void PriorityQueue::updateDependenciesFinish(const PacketData& packet) {
  assert(act_total_workers_count_ > 0);
  act_total_workers_count_--;

  packets_queues_[packet.priority_].decrementActWorkersCount();

  // Process all dependencies here - it is called when packet processing is finished

  if (packet.type_ == PriorityQueuePacketType::PQ_GetBlocksPacket || packet.type_ == PriorityQueuePacketType::PQ_BlocksPacket ||
      packet.type_ == PriorityQueuePacketType::PQ_PbftBlockPacket) {
    markPacketAsUnblocked(packet.type_);
  }
}

}  // namespace taraxa::network::tarcap