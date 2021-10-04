#pragma once

#include <libdevcore/RLP.h>

#include <array>
#include <utility>

#include "logger/logger.hpp"
#include "network/tarcap/packet_types.hpp"
#include "network/tarcap/threadpool/packets_blocking_mask.hpp"
#include "packets_queue.hpp"

namespace taraxa::network::tarcap {

class PriorityQueue {
 public:
  PriorityQueue(size_t tp_workers_count, const addr_t& node_addr = {});

  /**
   * @brief Pushes new packet into the priority queue
   * @param packet
   */
  void pushBack(PacketData&& packet);

  /**
   * @return std::optional<PacketData> packet with the highest priority & oldest "receive" time
   */
  std::optional<PacketData> pop();

  /**
   * @return true of all priority packets_queues_ are empty, otheriwse false
   */
  bool empty() const;

  /**
   * @brief Updates blocking dependencies at the start of packet processing
   *
   * @param packet
   */
  void updateDependenciesStart(const PacketData& packet);

  /**
   * @brief Updates blocking dependencies after packet processing is done
   *
   * @param packet
   * @param queue_mutex_ must be locked if processing also blocking dependencies
   * @param cond_var notify function is called if processing also blocking dependencies
   */
  void updateDependenciesFinish(const PacketData& packet, std::mutex& queue_mutex, std::condition_variable& cond_var);

 private:
  // Declare logger instances
  LOG_OBJECTS_DEFINE

  // Queues that group packets by it's priority.
  // All packets with PacketPriority::High go to packets_queues_[PacketPriority::High], etc...
  std::array<PacketsQueue, PacketData::PacketPriority::Count> packets_queues_{PacketsQueue(), PacketsQueue(),
                                                                              PacketsQueue()};

  // Mask with all packets types that are currently blocked for processing in another threads due to dependencies, e.g.
  // syncing packets must be processed synchronously one by one, etc...
  PacketsBlockingMask blocked_packets_mask_;

  // How many workers can process packets from all the queues at the same time
  const size_t MAX_TOTAL_WORKERS_COUNT;

  // How many workers are currently processing packets from all the queues at the same time
  std::atomic<size_t> act_total_workers_count_;
};

}  // namespace taraxa::network::tarcap
