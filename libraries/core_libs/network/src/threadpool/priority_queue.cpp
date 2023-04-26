#include "network/threadpool/priority_queue.hpp"

namespace taraxa::network::threadpool {

PriorityQueue::PriorityQueue(size_t tp_workers_count, const addr_t& node_addr)
    : blocked_packets_mask_(), MAX_TOTAL_WORKERS_COUNT(tp_workers_count), act_total_workers_count_(0) {
  assert(packets_queues_.size() == PacketData::PacketPriority::Count);
  // tp_workers_count value should be validated (>=3) after it is read from config
  assert(tp_workers_count >= 3);

  LOG_OBJECTS_CREATE("PRIORITY_QUEUE");

  // high priority packets(consensus - votes) max concurrent workers - 40% of tp_workers_count
  // mid priority packets(dag/pbft blocks, txs) max concurrent workers - 40% of tp_workers_count
  // low priority packets(syncing, status, ...) max concurrent workers - 30% of tp_workers_count
  size_t high_priority_queue_workers = std::max(1, static_cast<int>(MAX_TOTAL_WORKERS_COUNT * 4 / 10));
  size_t mid_priority_queue_workers = std::max(1, static_cast<int>(MAX_TOTAL_WORKERS_COUNT * 4 / 10));
  size_t low_priority_queue_workers = std::max(1, static_cast<int>(MAX_TOTAL_WORKERS_COUNT * 3 / 10));

  // It should not be possible to get into a situation when there is not at least 1 free thread for low priority queue
  assert(high_priority_queue_workers + mid_priority_queue_workers < MAX_TOTAL_WORKERS_COUNT);

  packets_queues_[PacketData::PacketPriority::High].setMaxWorkersCount(high_priority_queue_workers);
  packets_queues_[PacketData::PacketPriority::Mid].setMaxWorkersCount(mid_priority_queue_workers);
  packets_queues_[PacketData::PacketPriority::Low].setMaxWorkersCount(low_priority_queue_workers);

  LOG(log_nf_) << "Priority queues initialized accordingly: "
               << "total num of workers = " << MAX_TOTAL_WORKERS_COUNT
               << ", High priority packets max num of workers = " << high_priority_queue_workers
               << ", Mid priority packets max num of workers = " << mid_priority_queue_workers
               << ", Low priority packets max num of workers = " << low_priority_queue_workers;
}

void PriorityQueue::pushBack(PacketData&& packet) { packets_queues_[packet.priority_].pushBack(std::move(packet)); }

bool PriorityQueue::canBorrowThread() {
  size_t reserved_threads_num = 0;

  for (const auto& queue : packets_queues_) {
    // No need to reserve thread for this queue as it is using at least 1 thread at the moment
    if (queue.getActiveWorkersNum()) {
      continue;
    }

    reserved_threads_num++;
  }

  return act_total_workers_count_ < (MAX_TOTAL_WORKERS_COUNT - reserved_threads_num);
}

std::optional<PacketData> PriorityQueue::pop() {
  if (act_total_workers_count_ >= MAX_TOTAL_WORKERS_COUNT) {
    LOG(log_dg_) << "MAX_TOTAL_WORKERS_COUNT(" << MAX_TOTAL_WORKERS_COUNT << ") reached, unable to pop data.";
    return {};
  }

  // Flag if second iteration over queues should be done.
  // It can happen that no packet for processing was returned during the first iteration over priority queues as there
  // are limits for max total workers per each priority queue. These limits can and should be ignored in some
  // scenarios... For example:
  //
  // High priority queue reached it's max workers limit, other queues have inside many blocked packets that cannot be
  // currently processed concurrently and MAX_TOTAL_WORKERS_COUNT is not reached yet. In such case some threads might
  // be unused. In such cases priority queues max workers limits can and should be ignored
  bool try_borrow_thread = false;

  // Get first packet to be processed. Queues are ordered by priority
  // starting with highest priority and ending with lowest priority
  for (auto& queue : packets_queues_) {
    if (queue.empty()) {
      continue;
    }

    if (queue.maxWorkersCountReached()) {
      try_borrow_thread = true;
      continue;
    }

    if (auto packet = queue.pop(blocked_packets_mask_); packet.has_value()) {
      return packet;
    }

    // All packets in this queue are currently blocked
  }

  if (!try_borrow_thread) {
    LOG(log_dg_) << "No non-blocked packets to be processed.";
    return {};
  }

  if (!canBorrowThread()) {
    LOG(log_dg_) << "No non-blocked packets to be processed + limits reached -> unable to borrow thread due to "
                    "\"Always keep at least 1 reserved thread for each priority queue \" rule";
    return {};
  }

  // Second iteration over priority queues ignoring the max workers limits
  for (auto& queue : packets_queues_) {
    if (queue.empty()) {
      continue;
    }

    if (auto packet = queue.pop(blocked_packets_mask_); packet.has_value()) {
      LOG(log_dg_) << "Thread for packet processing borrowed";
      return packet;
    }

    // All packets in this queue are currently blocked
  }

  // There was no unblocked packet to be processed in all queues
  LOG(log_dg_) << "No non-blocked packets to be processed.";
  return {};
}

bool PriorityQueue::empty() const {
  return std::all_of(packets_queues_.cbegin(), packets_queues_.cend(), [](const auto& queue) { return queue.empty(); });
}

void PriorityQueue::updateDependenciesStart(const PacketData& packet) {
  assert(act_total_workers_count_ < MAX_TOTAL_WORKERS_COUNT);
  act_total_workers_count_++;

  packets_queues_[packet.priority_].incrementActWorkersCount();

  // Process all dependencies here - it is called when packet processing has started
  // !!! Important - there is a "mirror" function updateDependenciesFinish and all dependencies that are set
  // here should be unset in updateDependenciesFinish

  switch (packet.type_) {
    // Packets that can be processed only 1 at the time
    //  GetDagSyncPacket -> serve dag syncing data to only 1 node at the time
    //  GetPbftSyncPacket -> serve pbft syncing data to only 1 node at the time
    //  PbftSyncPacket -> process sync pbft blocks synchronously
    case SubprotocolPacketType::GetDagSyncPacket:
    case SubprotocolPacketType::GetPbftSyncPacket:
    case SubprotocolPacketType::PbftSyncPacket:
      blocked_packets_mask_.markPacketAsHardBlocked(packet, packet.type_);
      break;

    //  When syncing dag blocks, process only 1 packet at a time:
    //  DagSyncPacket -> process sync dag blocks synchronously
    //  DagBlockPacket -> wait with processing of new dag blocks until old blocks are synced
    case SubprotocolPacketType::DagSyncPacket:
      blocked_packets_mask_.markPacketAsHardBlocked(packet, packet.type_);
      blocked_packets_mask_.markPacketAsPeerOrderBlocked(packet, SubprotocolPacketType::DagBlockPacket);
      break;

    // When processing TransactionPacket, processing of all dag block packets that were received after that (from the
    // same peer). No need to block processing of dag blocks packets received before as it should not be possible to
    // send dag block before sending txs it contains...
    case SubprotocolPacketType::TransactionPacket:
      blocked_packets_mask_.markPacketAsPeerOrderBlocked(packet, SubprotocolPacketType::DagBlockPacket);
      break;

    case SubprotocolPacketType::DagBlockPacket:
      blocked_packets_mask_.setDagBlockLevelBeingProcessed(packet);
      blocked_packets_mask_.setDagBlockBeingProcessed(packet);
      break;

    default:
      break;
  }
}

void PriorityQueue::updateDependenciesFinish(const PacketData& packet, std::mutex& queue_mutex,
                                             std::condition_variable& cond_var) {
  assert(act_total_workers_count_ > 0);

  // Process all dependencies here - it is called when packet processing is finished

  // Note: every case in this switch must lock queue_mutex !!!
  switch (packet.type_) {
    case SubprotocolPacketType::GetDagSyncPacket:
    case SubprotocolPacketType::GetPbftSyncPacket:
    case SubprotocolPacketType::PbftSyncPacket: {
      std::unique_lock<std::mutex> lock(queue_mutex);
      blocked_packets_mask_.markPacketAsHardUnblocked(packet, packet.type_);
      cond_var.notify_all();
      break;
    }

    case SubprotocolPacketType::DagSyncPacket: {
      std::unique_lock<std::mutex> lock(queue_mutex);
      blocked_packets_mask_.markPacketAsHardUnblocked(packet, packet.type_);
      blocked_packets_mask_.markPacketAsPeerOrderUnblocked(packet, SubprotocolPacketType::DagBlockPacket);
      cond_var.notify_all();
      break;
    }

    case SubprotocolPacketType::TransactionPacket: {
      std::unique_lock<std::mutex> lock(queue_mutex);
      blocked_packets_mask_.markPacketAsPeerOrderUnblocked(packet, SubprotocolPacketType::DagBlockPacket);
      cond_var.notify_all();
      break;
    }

    case SubprotocolPacketType::DagBlockPacket: {
      std::unique_lock<std::mutex> lock(queue_mutex);
      blocked_packets_mask_.unsetDagBlockLevelBeingProcessed(packet);
      blocked_packets_mask_.unsetDagBlockBeingProcessed(packet);
      cond_var.notify_all();
      break;
    }

    default:
      break;
  }

  act_total_workers_count_--;
  packets_queues_[packet.priority_].decrementActWorkersCount();
}

size_t PriorityQueue::getPrirotityQueueSize(PacketData::PacketPriority priority) const {
  return packets_queues_[priority].size();
}

}  // namespace taraxa::network::threadpool
