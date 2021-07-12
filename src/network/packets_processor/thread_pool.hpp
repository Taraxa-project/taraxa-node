#pragma once

#include <chrono>
#include <condition_variable>
#include <functional>
#include <iostream>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

#include "network/packets_processor/priority_queue.hpp"

namespace taraxa::network {

/**
 * @brief Taraxa ThreadPool that is supposed to process incoming packets in concurrent way
 **/
class ThreadPool {
 public:
  /**
   * @param workers_num  Number of workers
   **/
  ThreadPool(size_t workers_num = 10);
  ~ThreadPool();

  ThreadPool(const ThreadPool&) = delete;
  ThreadPool& operator=(const ThreadPool&) = delete;
  ThreadPool(ThreadPool&&) = delete;
  ThreadPool& operator=(ThreadPool&&) = delete;

  /**
   * @brief Push the given element value to the end of the queue. Used for r-values
   **/
  void push(SubprotocolPacketType packet_type, dev::RLP&& packet_rlp);

  void stopProcessing();

  /**
   * @brief Threadpool sycnchronized processing function, which internally calls packet-specific handlers
   **/
  void processPacket(size_t worker_id);

 private:
  PacketType mapSubprotocolToQueuePacketType(SubprotocolPacketType packet_type);
  PacketPriority getPacketPriority(PacketType packet_type);

 private:
  // If true, stop processing packets and join all workers threads
  bool stopProcessing_;

  // Queue of unprocessed packets
  PriorityQueue queue_;

  // Queue mutex
  std::mutex mutex_;

  // Queue condition variable
  std::condition_variable cond_var_;

  // Vector of worker threads - should be initialized as the last member
  std::vector<std::thread> workers_;
};

}  // namespace taraxa::network
