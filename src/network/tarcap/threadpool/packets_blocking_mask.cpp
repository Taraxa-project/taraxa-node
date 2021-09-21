#include "packets_blocking_mask.hpp"

#include "dag/dag_block.hpp"

namespace taraxa::network::tarcap {

void PacketsBlockingMask::markPacketAsHardBlocked(PriorityQueuePacketType packet_type) {
  // blocked_packets_peers_time_ contains peers_time blocks that are not compatible with hard blocks, we should
  // not try to create hard block (and later unblock) when there is ongoing peers_time block
  assert(blocked_packets_peers_time_.find(packet_type) == blocked_packets_peers_time_.end());

  assert(!(blocked_packets_types_mask_ & packet_type));
  blocked_packets_types_mask_ |= packet_type;
}

void PacketsBlockingMask::markPacketAsHardUnblocked(PriorityQueuePacketType packet_type) {
  assert(blocked_packets_types_mask_ & packet_type);
  blocked_packets_types_mask_ ^= packet_type;
}

void PacketsBlockingMask::markPacketAsPeerTimeBlocked(const PacketData& blocking_packet,
                                                      PriorityQueuePacketType packet_to_be_blocked) {
  // blocked_packets_types_mask_ contains hard blocks that are not compatible with peers_time blocks, we should
  // not try to create peers time block when there is ongoing hard block
  assert(!(blocked_packets_types_mask_ & packet_to_be_blocked));

  // There is no existing peer time block for specific packet_type & peer_id, create new one
  auto& peer_time_blocks = blocked_packets_peers_time_[packet_to_be_blocked];
  if (!peer_time_blocks.count(blocking_packet.from_node_id_)) {
    peer_time_blocks[blocking_packet.from_node_id_] = {1, {blocking_packet.receive_time_}};
    return;
  }

  // There is existing peer time block for specific packet_type & peer_id, update it
  auto& time_block = peer_time_blocks[blocking_packet.from_node_id_];
  time_block.concurrent_processing_count_++;
  time_block.times_.insert(blocking_packet.receive_time_);
}

void PacketsBlockingMask::markPacketAsPeerTimeUnblocked(const PacketData& blocking_packet,
                                                        PriorityQueuePacketType packet_to_be_unblocked) {
  auto blocked_packet_peers_time = blocked_packets_peers_time_.find(packet_to_be_unblocked);
  assert(blocked_packet_peers_time != blocked_packets_peers_time_.end());

  auto peers_time_block = blocked_packet_peers_time->second.find(blocking_packet.from_node_id_);
  assert(peers_time_block != blocked_packet_peers_time->second.end());

  assert(peers_time_block->second.concurrent_processing_count_);

  // Do not delete time_block if there is currently multiple packets (of the same type & same peer) being processed
  if (peers_time_block->second.concurrent_processing_count_ > 1) {
    peers_time_block->second.concurrent_processing_count_--;

    assert(peers_time_block->second.times_.find(blocking_packet.receive_time_) !=
           peers_time_block->second.times_.end());
    peers_time_block->second.times_.erase(blocking_packet.receive_time_);
    return;
  }

  // Delete time_block once the last packet (of the same type & same peer) is processed
  blocked_packet_peers_time->second.erase(peers_time_block);

  // TODO: in case blocked_packet_peers_time.empty() == true, we might even erase
  // blocked_packets_peers_time_.erase(packet_data.type_)
  //       but it might be less efficient than keeping it there all the time. On the other isPacketBlocked might need to
  //       evaluate 1 less condition if it is erased...
}

std::optional<taraxa::level_t> PacketsBlockingMask::getSmallestDagLevelBeingProcessed() const {
  if (!processing_dag_levels_.empty()) {
    return {processing_dag_levels_.begin()->first};
  }

  return {};
}

void PacketsBlockingMask::setDagBlockLevelBeingProcessed(const PacketData& packet) {
  level_t dag_level = DagBlock::extract_dag_level_from_rlp(packet.rlp_[0]);

  const auto smallest_processing_dag_level = getSmallestDagLevelBeingProcessed();
  if (smallest_processing_dag_level.has_value()) {
    // Only new dag blocks with smaller or equal level than the smallest level from all blocks currently being processed
    // are allowed
    assert(dag_level <= smallest_processing_dag_level.value());

    // Adjust number of concurrent processing count for the current existing smallest dag level being provessed
    if (dag_level == smallest_processing_dag_level.value()) {
      processing_dag_levels_[dag_level]++;
    }
  }

  // Create new smallest dag level
  processing_dag_levels_.emplace(dag_level, 1);
}

void PacketsBlockingMask::unsetDagBlockLevelBeingProcessed(const PacketData& packet) {
  level_t dag_block_level = DagBlock::extract_dag_level_from_rlp(packet.rlp_[0]);

  const auto processing_dag_level = processing_dag_levels_.find(dag_block_level);

  // There must be existing dag level inside processing_dag_levels_
  assert(processing_dag_level != processing_dag_levels_.end());

  // Concurrent processing count for specified dag level must be >= 1
  assert(processing_dag_level->second >= 1);

  if (processing_dag_level->second > 1) {
    processing_dag_level->second--;
    return;
  }

  processing_dag_levels_.erase(processing_dag_level);
}

bool PacketsBlockingMask::isPacketBlocked(const PacketData& packet_data) const {
  // If packet is hard blocked, no need to check custom packet blocks or peer time blocks
  if (packet_data.type_ & blocked_packets_types_mask_) {
    return true;
  }

  // Custom packet types blocks
  if (packet_data.type_ == PriorityQueuePacketType::kPqNewBlockPacket) {
    if (const auto smallest_processing_dag_level = getSmallestDagLevelBeingProcessed();
        smallest_processing_dag_level.has_value()) {
      const auto dag_level = DagBlock::extract_dag_level_from_rlp(packet_data.rlp_[0]);
      if (dag_level > smallest_processing_dag_level.value()) {
        return true;
      }
    }
  }

  // There is no peers_time block for packet_data.type_ packet type
  auto blocked_packet_peers_time = blocked_packets_peers_time_.find(packet_data.type_);
  if (blocked_packet_peers_time == blocked_packets_peers_time_.end()) {
    return false;
  }

  // There is no peers_time block for packet_data.from_node_id_ peer
  auto peers_time_block = blocked_packet_peers_time->second.find(packet_data.from_node_id_);
  if (peers_time_block == blocked_packet_peers_time->second.end()) {
    return false;
  }

  // There is peers_time block for packet_data.from_node_id_ packet type and packet_data.from_node_id_ peer
  // Block all packets that were received after peers_time block timestamp
  if (packet_data.receive_time_ >= *peers_time_block->second.times_.begin()) {
    return true;
  }

  return false;
}

}  // namespace taraxa::network::tarcap