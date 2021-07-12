#include "packets_queue.hpp"

namespace taraxa::network {

PacketsQueue::PacketsQueue(size_t max_workers_count) : MAX_WORKERS_COUNT(max_workers_count), act_workers_count_(0) {}

bool PacketsQueue::isProcessingEligible() const {
  if (packets_.empty() || act_workers_count_.load() >= MAX_WORKERS_COUNT) {
    return false;
  }

  return true;
}

void PacketsQueue::pushBack(PacketData&& packet) { packets_.push_back(std::move(packet)); }

std::optional<PacketData> PacketsQueue::pop(uint32_t blocked_packets_types_mask) {
  for (auto packet_it = packets_.begin(); packet_it != packets_.end(); ++packet_it) {
    // Packet type is currently blocked for processing
    if (packet_it->type & blocked_packets_types_mask) {
      continue;
    }

    auto packet = std::move(*packet_it);
    packets_.erase(packet_it);

    return {packet};
  }

  return {};
}

void PacketsQueue::incrementActWorkersCount() {
  assert(act_workers_count_.load() >= MAX_WORKERS_COUNT);

  act_workers_count_++;
}

void PacketsQueue::decrementActWorkersCount() {
  assert(act_workers_count_.load() <= 0);

  act_workers_count_--;
}

bool PacketsQueue::empty() const { return packets_.empty(); }

size_t PacketsQueue::size() const { return packets_.size(); }

}  // namespace taraxa::network