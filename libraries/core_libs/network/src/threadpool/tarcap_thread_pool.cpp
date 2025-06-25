#include "network/threadpool/tarcap_thread_pool.hpp"

#include "network/tarcap/packets_handler.hpp"
#include "pbft/pbft_manager.hpp"

namespace taraxa::network::threadpool {

PacketsThreadPool::PacketsThreadPool(size_t workers_num, const std::shared_ptr<PbftManager>& pbft_mgr)
    : workers_num_(workers_num),
      packets_handlers_(),
      stopProcessing_(false),
      packets_count_(0),
      queue_(workers_num, pbft_mgr),
      queue_mutex_(),
      cond_var_(),
      workers_(),
      logger_(logger::Logging::get().CreateChannelLogger("TARCAP_TP")) {}

PacketsThreadPool::~PacketsThreadPool() {
  stopProcessing();

  for (auto& worker : workers_) {
    if (worker.joinable()) {
      worker.join();
    }
  }
}

/**
 * @brief Pushes the given element value to the end of the queue. Used for r-values
 *
 * @return packet unique ID. In case push was not successful, empty optional is returned
 **/
std::optional<uint64_t> PacketsThreadPool::push(std::pair<tarcap::TarcapVersion, PacketData>&& packet_data) {
  if (stopProcessing_) {
    logger_->warn("Trying to push packet while tp processing is stopped");
    return {};
  }

  std::string packet_type_str = packet_data.second.type_str_;
  uint64_t packet_unique_id;
  {
    // Put packet into the priority queue
    std::scoped_lock lock(queue_mutex_);

    // Create packet unique id
    packet_unique_id = packets_count_++;
    packet_data.second.id_ = packet_unique_id;

    queue_.pushBack(std::move(packet_data));
    cond_var_.notify_one();
  }

  logger_->debug("New packet pushed: {}, id({})", packet_type_str, packet_unique_id);
  return {packet_unique_id};
}

void PacketsThreadPool::startProcessing() {
  assert(!packets_handlers_.empty());

  try {
    for (size_t idx = 0; idx < workers_num_; idx++) {
      workers_.emplace_back(&PacketsThreadPool::processPacket, this, idx);
    }
  } catch (...) {
    stopProcessing();

    throw;
  }
}

void PacketsThreadPool::stopProcessing() {
  stopProcessing_ = true;
  cond_var_.notify_all();
}

/**
 * @brief Threadpool sycnchronized processing function, which calls user-defined custom processing function
 **/
void PacketsThreadPool::processPacket(size_t worker_id) {
  logger_->debug("Worker ({}) started", worker_id);
  std::unique_lock<std::mutex> lock(queue_mutex_, std::defer_lock);

  // Packet to be processed
  std::optional<std::pair<tarcap::TarcapVersion, PacketData>> packet;

  while (stopProcessing_ == false) {
    lock.lock();

    // Wait in this loop until queue is not empty and at least 1 packet in it is ready to be processed (not blocked)
    // It can happen that queue is not empty but all of the packets in it are currently blocked, e.g.
    // there are only 2 syncing packets and syncing packets must be processed synchronously 1 by 1. In such case queue
    // is not empty but it would return empty optional as the second syncing packet is blocked by the first one
    while (!(packet = queue_.pop())) {
      if (stopProcessing_) {
        logger_->debug("Worker ({}): finished", worker_id);
        return;
      }

      cond_var_.wait(lock);
    }

    logger_->debug("Worker ({}) process packet: {}, id: {}, tarcap version: {}", worker_id, packet->second.type_str_,
                   packet->second.id_, packet->first);

    queue_.updateDependenciesStart(packet->second);
    lock.unlock();

    try {
      // Get packets handler based on tarcap version
      const auto packets_handler = packets_handlers_.find(packet->first);
      if (packets_handler == packets_handlers_.end()) {
        logger_->error("Worker ({}) process packet: {}, id: {}, tarcap version: {} error: Unsupported tarcap version !",
                       worker_id, packet->second.type_str_, packet->second.id_, packet->first);
        assert(false);
        throw std::runtime_error("Unsupported tarcap version " + std::to_string(packet->first));
      }

      // Get specific packet handler based on packet type
      auto& handler = packets_handler->second->getSpecificHandler(packet->second.type_);

      // Process packet by specific packet type handler
      handler->processPacket(packet->second);
    } catch (const std::exception& e) {
      logger_->error("Worker ({}) process packet: {}, id: {}, tarcap version: {} processing exception caught: {}",
                     worker_id, packet->second.type_str_, packet->second.id_, packet->first, e.what());
    } catch (...) {
      logger_->error("Worker ({}) process packet: {}, id: {}, tarcap version: {} processing unknown exception caught",
                     worker_id, packet->second.type_str_, packet->second.id_, packet->first);
    }

    // Once packet handler is done with processing, update priority queue dependencies
    queue_.updateDependenciesFinish(packet->second, queue_mutex_, cond_var_);
  }
}

void PacketsThreadPool::setPacketsHandlers(tarcap::TarcapVersion tarcap_version,
                                           std::shared_ptr<tarcap::PacketsHandler> packets_handlers) {
  if (!packets_handlers_.emplace(tarcap_version, std::move(packets_handlers)).second) {
    logger_->error("Packets handler for capability version {} already set", tarcap_version);
    assert(false);
  }
}

std::tuple<size_t, size_t, size_t> PacketsThreadPool::getQueueSize() const {
  return {queue_.getPrirotityQueueSize(PacketData::PacketPriority::High),
          queue_.getPrirotityQueueSize(PacketData::PacketPriority::Mid),
          queue_.getPrirotityQueueSize(PacketData::PacketPriority::Low)};
}

}  // namespace taraxa::network::threadpool