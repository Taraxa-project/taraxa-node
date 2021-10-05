#include "network/tarcap/threadpool/packets_queue.hpp"

namespace taraxa::network::tarcap {

PacketsQueue::PacketsQueue(size_t max_workers_count)
    : packets_(), kMaxWorkersCount_(max_workers_count) {}

bool PacketsQueue::maxWorkersCountReached() const {
  if (act_workers_count_ >= kMaxWorkersCount_) {
    return false;
  }

  return true;
}

void PacketsQueue::pushBack(PacketData&& packet) {
  packets_.push_back(std::move(packet));
  act_packets_count_++;
}

std::optional<PacketData> PacketsQueue::pop(const PacketsBlockingMask& packets_blocking_mask) {
  for (auto packet_it = packets_.begin(); packet_it != packets_.end(); ++packet_it) {
    // Packet type is currently blocked for processing
    if (packets_blocking_mask.isPacketBlocked(*packet_it)) {
      continue;
    }

    std::optional<PacketData> ret = std::move(*packet_it);
    packets_.erase(packet_it);
    act_packets_count_--;

    return ret;
  }

  return {};
}

void PacketsQueue::setMaxWorkersCount(size_t max_workers_count) { kMaxWorkersCount_ = max_workers_count; }

void PacketsQueue::incrementActWorkersCount() { act_workers_count_++; }

void PacketsQueue::decrementActWorkersCount() {
  assert(act_workers_count_ > 0);

  act_workers_count_--;
}

bool PacketsQueue::empty() const { return act_packets_count_ == 0; }

size_t PacketsQueue::size() const { return act_packets_count_; }

}  // namespace taraxa::network::tarcap