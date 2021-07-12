#include "thread_pool.hpp"

namespace taraxa::network {

ThreadPool::ThreadPool(size_t workers_num) : stopProcessing_(false), queue_(), mutex_(), cond_var_(), workers_() {
  try {
    for (size_t idx = 0; idx < workers_num; idx++) {
      workers_.emplace_back(&ThreadPool::processPacket, this, idx);
    }
  } catch (...) {
    stopProcessing_ = true;

    throw;
  }
}

ThreadPool::~ThreadPool() {
  stopProcessing();

  for (auto& worker : workers_) {
    if (worker.joinable()) {
      worker.join();
    }
  }
}

PacketType ThreadPool::mapSubprotocolToQueuePacketType(SubprotocolPacketType packet_type) {
  switch (packet_type) {
    case SubprotocolPacketType::StatusPacket:
      return PacketType::_StatusPacket;
    case SubprotocolPacketType::NewBlockPacket:
      return PacketType::_NewBlockPacket;
    case SubprotocolPacketType::NewBlockHashPacket:
      return PacketType::_NewBlockHashPacket;
    case SubprotocolPacketType::GetNewBlockPacket:
      return PacketType::_GetNewBlockPacket;
    case SubprotocolPacketType::GetBlocksPacket:
      return PacketType::_GetBlocksPacket;
    case SubprotocolPacketType::BlocksPacket:
      return PacketType::_BlocksPacket;
    case SubprotocolPacketType::TransactionPacket:
      return PacketType::_TransactionPacket;
    case SubprotocolPacketType::TestPacket:
      return PacketType::_TestPacket;
    case SubprotocolPacketType::PbftVotePacket:
      return PacketType::_PbftVotePacket;
    case SubprotocolPacketType::GetPbftNextVotes:
      return PacketType::_GetPbftNextVotes;
    case SubprotocolPacketType::PbftNextVotesPacket:
      return PacketType::_PbftNextVotesPacket;
    case SubprotocolPacketType::NewPbftBlockPacket:
      return PacketType::_NewPbftBlockPacket;
    case SubprotocolPacketType::GetPbftBlockPacket:
      return PacketType::_GetPbftBlockPacket;
    case SubprotocolPacketType::PbftBlockPacket:
      return PacketType::_PbftBlockPacket;
    case SubprotocolPacketType::PacketCount:
      return PacketType::_PacketCount;
    case SubprotocolPacketType::SyncedPacket:
      return PacketType::_SyncedPacket;
    case SubprotocolPacketType::SyncedResponsePacket:
      return PacketType::_SyncedResponsePacket;
    default:
      assert(false);
  }
}

PacketPriority ThreadPool::getPacketPriority(PacketType packet_type) {
  if (packet_type > PacketType::_HighPriorityPackets && packet_type < PacketType::_MidPriorityPackets) {
    return PacketPriority::High;
  } else if (packet_type > PacketType::_MidPriorityPackets && packet_type < PacketType::_LowPriorityPackets) {
    return PacketPriority::Mid;
  } else if (packet_type > PacketType::_LowPriorityPackets) {
    return PacketPriority::Low;
  }

  assert(false);
}

/**
 * @brief Pushes the given element value to the end of the queue. Used for r-values
 **/
void ThreadPool::push(SubprotocolPacketType packet_type, dev::RLP&& packet_rlp) {
  if (stopProcessing_) {
    return;
  }
  // note: in case we want to drop some packets for any reason, it can be implemented here

  auto queue_packet_type = mapSubprotocolToQueuePacketType(packet_type);

  std::scoped_lock lock(mutex_);
  queue_.pushBack({queue_packet_type, getPacketPriority(queue_packet_type), std::move(packet_rlp)});
  cond_var_.notify_one();
}

void ThreadPool::stopProcessing() {
  stopProcessing_ = true;
  cond_var_.notify_all();
}

/**
 * @brief Threadpool sycnchronized processing function, which calls user-defined custom processing function
 **/
void ThreadPool::processPacket(size_t worker_id) {
  std::unique_lock<std::mutex> lock(mutex_, std::defer_lock);

  while (stopProcessing_ == false) {
    lock.lock();

    while (queue_.empty()) {
      if (stopProcessing_) {
        return;
      }

      cond_var_.wait(lock);
    }

    // Get packet with highest priority & oldest "receive" time
    auto packet = queue_.pop();

    // This can happen if there are some packets to be processed in queue but all of them are currently blocked, e.g.
    // there are only 2 syncing packets and syncing packets must be processed synchronously 1 by 1. In such case queue
    // is not empty but it would return empty optional as the second syncing packet is blocked by the first one
    if (!packet.has_value()) {
      lock.unlock();

      // Sleep for some time and try to get the packet later again to see if it is still blocked
      std::this_thread::sleep_for(std::chrono::milliseconds(20));
      continue;
    }

    queue_.updateDependenciesStart(packet.value());
    lock.unlock();

    // Process packet by specific packet type handler
    //    try {
    //      // TODO: processor_(task.second);
    //    } catch (const std::exception& e) {
    //
    //    } catch (...) {
    //
    //    }

    // Once packet handler finish, update priority queue dependencies - no need to lock queue as deps are atomic
    queue_.updateDependenciesFinish(packet.value());
  }
}

}  // namespace taraxa::network