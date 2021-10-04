#include "network/tarcap/threadpool/tarcap_thread_pool.hpp"

#include "network/tarcap/packets_handler.hpp"

namespace taraxa::network::tarcap {

TarcapThreadPool::TarcapThreadPool(size_t workers_num, const addr_t& node_addr)
    : workers_num_(workers_num),
      packets_handlers_(nullptr),
      stopProcessing_(false),
      queue_(workers_num, node_addr),
      queue_mutex_(),
      cond_var_(),
      workers_() {
  LOG_OBJECTS_CREATE("TARCAP_TP");
}

TarcapThreadPool::~TarcapThreadPool() {
  stopProcessing();

  for (auto& worker : workers_) {
    if (worker.joinable()) {
      worker.join();
    }
  }
}

/**
 * @brief Pushes the given element value to the end of the queue. Used for r-values
 **/
void TarcapThreadPool::push(PacketData&& packet_data) {
  if (stopProcessing_) {
    return;
  }

  LOG(log_dg_) << "ThreadPool new packet pushed: " << packet_data.type_str_ << std::endl;

  // Put packet into the priority queue
  std::scoped_lock lock(queue_mutex_);
  queue_.pushBack(std::move(packet_data));
  cond_var_.notify_one();
}

void TarcapThreadPool::startProcessing() {
  assert(packets_handlers_ != nullptr);

  try {
    for (size_t idx = 0; idx < workers_num_; idx++) {
      workers_.emplace_back(&TarcapThreadPool::processPacket, this, idx);
    }
  } catch (...) {
    stopProcessing();

    throw;
  }
}

void TarcapThreadPool::stopProcessing() {
  stopProcessing_ = true;
  cond_var_.notify_all();
}

/**
 * @brief Threadpool sycnchronized processing function, which calls user-defined custom processing function
 **/
void TarcapThreadPool::processPacket(size_t worker_id) {
  LOG(log_dg_) << "Worker (" << worker_id << ") started";
  std::unique_lock<std::mutex> lock(queue_mutex_, std::defer_lock);

  // Packet to be processed
  std::optional<PacketData> packet;

  while (stopProcessing_ == false) {
    lock.lock();

    // Wait in this loop until queue is not empty and at least 1 packet in it is ready to be processed (not blocked)
    // It can happen that queue is not empty but all of the packets in it are currently blocked, e.g.
    // there are only 2 syncing packets and syncing packets must be processed synchronously 1 by 1. In such case queue
    // is not empty but it would return empty optional as the second syncing packet is blocked by the first one
    while (!(packet = queue_.pop())) {
      if (stopProcessing_) {
        LOG(log_dg_) << "Worker (" << worker_id << "): finished";
        return;
      }

      LOG(log_dg_) << "Worker (" << worker_id << ") waiting for packets to be processed.";
      cond_var_.wait(lock);
    }

    LOG(log_dg_) << "Worker (" << worker_id << ") process packet: " << packet->type_str_;

    queue_.updateDependenciesStart(packet.value());
    lock.unlock();

    try {
      // Get specific packet handler according to packet type
      auto& handler = packets_handlers_->getSpecificHandler(packet->type_);

      // Process packet by specific packet type handler
      handler->processPacket(packet.value());
    } catch (const std::exception& e) {
      LOG(log_er_) << "Worker (" << worker_id << ") packet processing exception caught: " << e.what();
    } catch (...) {
      LOG(log_er_) << "Worker (" << worker_id << ") packet processing unknown exception caught";
    }

    // Once packet handler is done with processing, update priority queue dependencies
    queue_.updateDependenciesFinish(packet.value(), queue_mutex_, cond_var_);
  }
}

void TarcapThreadPool::setPacketsHandlers(std::shared_ptr<PacketsHandler> packets_handlers) {
  packets_handlers_ = std::move(packets_handlers);
}

}  // namespace taraxa::network::tarcap