#include "packets_queue.hpp"

namespace taraxa::network::tarcap {

PacketsQueue::PacketsQueue(size_t max_workers_count) : packets_(), MAX_WORKERS_COUNT(max_workers_count), act_workers_count_(0) {}

bool PacketsQueue::isProcessingEligible() const {
  if (packets_.empty() || act_workers_count_ >= MAX_WORKERS_COUNT) {
    return false;
  }

  return true;
}

void PacketsQueue::pushBack(PacketData&& packet) { packets_.push_back(std::move(packet)); }

std::optional<PacketData> PacketsQueue::pop(const PacketsBlockingMask& packets_blocking_mask) {
  for (auto packet_it = packets_.begin(); packet_it != packets_.end(); ++packet_it) {
    // Packet type is currently blocked for processing
    if (packets_blocking_mask.isPacketBlocked(*packet_it)) {
      continue;
    }

    std::optional<PacketData> ret = std::move(*packet_it);
    packets_.erase(packet_it);

    return ret;
  }

  return {};
}

void PacketsQueue::setMaxWorkersCount(size_t max_workers_count) { MAX_WORKERS_COUNT = max_workers_count; }

void PacketsQueue::incrementActWorkersCount() {
  assert(act_workers_count_ < MAX_WORKERS_COUNT);

  act_workers_count_++;
}

void PacketsQueue::decrementActWorkersCount() {
  assert(act_workers_count_ > 0);

  act_workers_count_--;
}

bool PacketsQueue::empty() const { return packets_.empty(); }

size_t PacketsQueue::size() const { return packets_.size(); }

}  // namespace taraxa::network::tarcap