#pragma once

#include <list>
#include <optional>

#include "network/tarcap/threadpool/packets_blocking_mask.hpp"
#include "packet_data.hpp"

namespace taraxa::network::tarcap {

class PacketsQueue {
 public:
  PacketsQueue(size_t max_workers_count = 5);

  /**
   * @brief Push new task to the queue
   *
   * @param packet
   */
  void pushBack(PacketData&& packet);

  /**
   * @brief Return Task from queue. In some rare situations when all packets are blocked for processing due to
   *        blocking dependencies there might returned empty optional
   * @note If empty optional is returned too often, there might be some logical bug in terms of packet priority &
   *       existing dependencies
   * @param blocked_packets_types_mask bit mask with all blocked packets for processing
   *
   * @return std::optional<Task>
   */
  std::optional<PacketData> pop(const PacketsBlockingMask& packets_blocking_mask);

  /**
   * @return false in case there is already MAX_WORKERS_COUNT workers processing packets from
   *         this queue at the same time, otherwise true
   */
  bool maxWorkersCountReached() const;

  /**
   * @brief Set new max workers count
   *
   * @param max_workers_count
   */
  void setMaxWorkersCount(size_t max_workers_count);

  /**
   * @brief Increment act_workers_count_ by 1
   */
  void incrementActWorkersCount();

  /**
   * @brief Decrement act_workers_count_ by 1
   */
  void decrementActWorkersCount();

  /**
   * @return true if queue is empty, otherwise false
   */
  bool empty() const;

  /**
   * @return size of the queue
   */
  size_t size() const;

 private:
  std::list<PacketData> packets_;

  // How many workers can process packets from this queue at the same time
  size_t MAX_WORKERS_COUNT;

  // How many workers are currently processing packets from this queue at the same time
  std::atomic<size_t> act_workers_count_;
};

}  // namespace taraxa::network::tarcap
