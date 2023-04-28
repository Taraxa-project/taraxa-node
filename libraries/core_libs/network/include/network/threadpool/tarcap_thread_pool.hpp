#pragma once

#include <condition_variable>
#include <mutex>
#include <thread>
#include <vector>

#include "logger/logger.hpp"
#include "network/tarcap/tarcap_version.hpp"
#include "priority_queue.hpp"

namespace taraxa::network::tarcap {
class PacketsHandler;
}

namespace taraxa::network::threadpool {

/**
 * @brief PacketsThreadPool for concurrent packets processing
 **/
class PacketsThreadPool {
 public:
  /**
   * @param workers_num  Number of workers
   **/
  PacketsThreadPool(size_t workers_num = 10, const addr_t& node_addr = {});
  ~PacketsThreadPool();

  PacketsThreadPool(const PacketsThreadPool&) = delete;
  PacketsThreadPool& operator=(const PacketsThreadPool&) = delete;
  PacketsThreadPool(PacketsThreadPool&&) = delete;
  PacketsThreadPool& operator=(PacketsThreadPool&&) = delete;

  /**
   * @brief Push the given element value to the end of the queue. Used for r-values
   *
   * @return packet unique ID. In case push was not successful, empty optional is returned
   **/
  std::optional<uint64_t> push(std::pair<tarcap::TarcapVersion, PacketData>&& packet_data);

  /**
   * @brief Start all processing threads
   */
  void startProcessing();

  /**
   * @brief Stop all processing threads
   */
  void stopProcessing();

  /**
   * @brief Threadpool sycnchronized processing function, which internally calls packet-specific handlers
   **/
  void processPacket(size_t worker_id);

  /**
   * @brief Sets packet handler
   *
   * @param packets_handlers
   */
  void setPacketsHandlers(tarcap::TarcapVersion tarcap_version,
                          std::shared_ptr<tarcap::PacketsHandler> packets_handlers);

  /**
   * @brief Returns actual size of all priority queues (thread-safe)
   *
   * @return std::tuple<size_t, size_t, size_t> - > std::tuple<HighPriorityQueue.size(), MidPriorityQueue.size(),
   * LowPriorityQueue.size()>
   */
  std::tuple<size_t, size_t, size_t> getQueueSize() const;

 private:
  // Declare logger instances
  LOG_OBJECTS_DEFINE

  // Number of workers(threads)
  const size_t workers_num_;

  // Common packets handler - each taraxa capability haits own packets handler
  std::unordered_map<tarcap::TarcapVersion, std::shared_ptr<tarcap::PacketsHandler>> packets_handlers_;

  // If true, stop processing packets and join all workers threads
  std::atomic<bool> stopProcessing_{false};

  // How many packets were pushed into the queue, it also serves for creating packet unique id
  uint64_t packets_count_{0};

  // Queue of unprocessed packets
  PriorityQueue queue_;

  // Queue mutex
  std::mutex queue_mutex_;

  // Queue condition variable
  std::condition_variable cond_var_;

  // Vector of worker threads - should be initialized as the last member
  std::vector<std::thread> workers_;
};

}  // namespace taraxa::network::threadpool
