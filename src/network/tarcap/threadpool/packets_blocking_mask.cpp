#include "packets_blocking_mask.hpp"

namespace taraxa::network::tarcap {

void PacketsBlockingMask::markPacketAsHardBlocked(PriorityQueuePacketType packet_type) {
  // time_blocked_packets_ contains time blocks that are not compatible with hard blocks, we should
  // not try to create hard block (and later unblock) when there is ongoing time block
  assert(time_blocked_packets_.find(packet_type) == time_blocked_packets_.end());

  assert(!(hard_blocked_packets_mask_ & packet_type));
  hard_blocked_packets_mask_ |= packet_type;
}

void PacketsBlockingMask::markPacketAsHardUnblocked(PriorityQueuePacketType packet_type) {
  assert(hard_blocked_packets_mask_ & packet_type);
  hard_blocked_packets_mask_ ^= packet_type;
}

static std::chrono::steady_clock::time_point lol = std::chrono::steady_clock::now();

void PacketsBlockingMask::markPacketAsTimeBlocked(const PacketData& blocking_packet,
                                                  PriorityQueuePacketType packet_to_be_blocked) {
  // hard_blocked_packets_mask_ contains hard blocks that are not compatible with time blocks, we should
  // not try to create time block when there is ongoing hard block
  assert(!(hard_blocked_packets_mask_ & packet_to_be_blocked));

  // Create (or update) time block for specific packet_type
  auto& time_block = time_blocked_packets_[packet_to_be_blocked];
  time_block.concurrent_processing_count_++;
  time_block.times_.insert(blocking_packet.receive_time_);
}

void PacketsBlockingMask::markPacketAsTimeUnblocked(const PacketData& blocking_packet,
                                                    PriorityQueuePacketType packet_to_be_unblocked) {
  auto time_blocked_packet = time_blocked_packets_.find(packet_to_be_unblocked);
  assert(time_blocked_packet != time_blocked_packets_.end());
  assert(time_blocked_packet->second.concurrent_processing_count_);
  assert(time_blocked_packet->second.times_.find(blocking_packet.receive_time_) !=
         time_blocked_packet->second.times_.end());

  // It is the last ongoing time block for specific packet type, delete it
  if (time_blocked_packet->second.concurrent_processing_count_ == 1) {
    time_blocked_packets_.erase(time_blocked_packet);
    return;
  }

  // There are other ongoing time blocks for specific packet type, update it
  time_blocked_packet->second.concurrent_processing_count_--;
  time_blocked_packet->second.times_.erase(blocking_packet.receive_time_);
}

bool PacketsBlockingMask::isPacketBlocked(const PacketData& packet_data) const {
  // If packet is hard blocked, no need to check time blocks
  if (packet_data.type_ & hard_blocked_packets_mask_) {
    return true;
  }

  // There is no time block for "packet_data.type_" packet type
  auto time_blocked_packet = time_blocked_packets_.find(packet_data.type_);
  if (time_blocked_packet == time_blocked_packets_.end()) {
    return false;
  }

  // There is time block for packet_data.from_node_id_ packet type
  // Block all packets that were received after time block timestamp
  if (packet_data.receive_time_ >= *time_blocked_packet->second.times_.begin()) {
    return true;
  }

  return false;
}

}  // namespace taraxa::network::tarcap