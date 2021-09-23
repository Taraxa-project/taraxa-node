#include "packets_blocking_mask.hpp"

#include "dag/dag_block.hpp"

namespace taraxa::network::tarcap {

void PacketsBlockingMask::markPacketAsHardBlocked(const PacketData& blocking_packet,
                                                  SubprotocolPacketType packet_type_to_block) {
  auto& packet_hard_block = hard_blocked_packet_types_[packet_type_to_block];

  // If this assert is triggered, it means we are trying to insert blocking packet id that is already inserted, which
  // should never happen as packets id's are supposed to be unique
  assert(packet_hard_block.insert(blocking_packet.id_).second);
}

void PacketsBlockingMask::markPacketAsHardUnblocked(const PacketData& blocking_packet,
                                                    SubprotocolPacketType packet_type_to_unblock) {
  const auto packet_hard_block = hard_blocked_packet_types_.find(packet_type_to_unblock);
  assert(packet_hard_block != hard_blocked_packet_types_.end());

  const auto found_blocking_packet = packet_hard_block->second.find(blocking_packet.id_);
  assert(found_blocking_packet != packet_hard_block->second.end());

  // Delete only found blocking packet block if there is currently multiple packets hard blocking this packet type
  if (packet_hard_block->second.size() > 1) {
    packet_hard_block->second.erase(found_blocking_packet);
    return;
  }

  // Delete whole packet_hard_block once the last blocking packet is processed
  hard_blocked_packet_types_.erase(packet_type_to_unblock);
}

void PacketsBlockingMask::markPacketAsPeerOrderBlocked(const PacketData& blocking_packet,
                                                       SubprotocolPacketType packet_type_to_block) {
  auto& packet_peer_order_blocks = peer_order_blocked_packet_types_[packet_type_to_block];
  auto& peer_order_blocks = packet_peer_order_blocks[blocking_packet.from_node_id_];

  // If this assert is triggered, it means we are trying to insert blocking packet id that is already inserted, which
  // should never happen as packets id's are supposed to be unique
  assert(peer_order_blocks.insert(blocking_packet.id_).second);
}

void PacketsBlockingMask::markPacketAsPeerOrderUnblocked(const PacketData& blocking_packet,
                                                         SubprotocolPacketType packet_type_to_unblock) {
  const auto packet_peer_order_block = peer_order_blocked_packet_types_.find(packet_type_to_unblock);
  assert(packet_peer_order_block != peer_order_blocked_packet_types_.end());

  const auto peer_order_block = packet_peer_order_block->second.find(blocking_packet.from_node_id_);
  assert(peer_order_block != packet_peer_order_block->second.end());

  const auto found_blocking_packet = peer_order_block->second.find(blocking_packet.id_);
  assert(found_blocking_packet != peer_order_block->second.end());

  // Do not delete peer_order_block if there is currently multiple packets (of the same type & same peer) being
  // processed, thus blocking processing of packet_type_to_unblock
  if (peer_order_block->second.size() > 1) {
    peer_order_block->second.erase(found_blocking_packet);
    return;
  }

  // Delete peer_order_block once the last packet (of the same type & same peer) is processed
  packet_peer_order_block->second.erase(peer_order_block);

  // Note: first level(SubprotocolPacketType) -> packet_peer_order_block blocks are not deleted as it is more effective
  // to keep it always in memory (even with 0 size) rather than deleting and creating it
}

std::optional<taraxa::level_t> PacketsBlockingMask::getSmallestDagLevelBeingProcessed() const {
  if (!processing_dag_levels_.empty()) {
    return {processing_dag_levels_.begin()->first};
  }

  return {};
}

void PacketsBlockingMask::setDagBlockLevelBeingProcessed(const PacketData& packet) {
  level_t dag_level = DagBlock::extract_dag_level_from_rlp(packet.rlp_[0]);

  // Only new dag blocks with smaller or equal level than the smallest level from all blocks currently being processed
  // are allowed
  if (const auto smallest_processing_dag_level = getSmallestDagLevelBeingProcessed();
      smallest_processing_dag_level.has_value()) {
    assert(dag_level <= smallest_processing_dag_level.value());
  }

  auto& processing_dag_level = processing_dag_levels_[dag_level];

  // If this assert is triggered, it means we are trying to insert blocking packet id that is already inserted, which
  // should never happen as packets id's are supposed to be unique
  assert(processing_dag_level.insert(packet.id_).second);
}

void PacketsBlockingMask::unsetDagBlockLevelBeingProcessed(const PacketData& packet) {
  level_t dag_block_level = DagBlock::extract_dag_level_from_rlp(packet.rlp_[0]);

  // There must be existing dag level inside processing_dag_levels_
  const auto processing_dag_level = processing_dag_levels_.find(dag_block_level);
  assert(processing_dag_level != processing_dag_levels_.end());

  // There must be existing specific packet_id inside processing_dag_level
  const auto found_blocking_packet = processing_dag_level->second.find(packet.id_);
  assert(found_blocking_packet != processing_dag_level->second.end());

  // Delete only blocking packet id if there is currently multiple NewDagBlock packets with the same dag level
  // being processed
  if (processing_dag_level->second.size() > 1) {
    processing_dag_level->second.erase(found_blocking_packet);
    return;
  }

  // Delete whole processing_dag_level once the last NewDagBlock packet with the same dag level is processed
  processing_dag_levels_.erase(processing_dag_level);
}

bool PacketsBlockingMask::isPacketHardBlocked(const PacketData& packet_data) const {
  // There is no peers_time block for packet_data.type_ packet type
  return hard_blocked_packet_types_.count(packet_data.type_);
}

bool PacketsBlockingMask::isDagBlockPacketBlockedByLevel(const PacketData& packet_data) const {
  const auto smallest_processing_dag_level = getSmallestDagLevelBeingProcessed();

  if (!smallest_processing_dag_level.has_value()) {
    return false;
  }

  const auto dag_level = DagBlock::extract_dag_level_from_rlp(packet_data.rlp_[0]);
  if (dag_level > smallest_processing_dag_level.value()) {
    return true;
  }

  return false;
}

bool PacketsBlockingMask::isPacketPeerOrderBlocked(const PacketData& packet_data) const {
  // There is no peer_order block for packet_data.type_
  const auto packet_peer_order_block = peer_order_blocked_packet_types_.find(packet_data.type_);
  if (packet_peer_order_block == peer_order_blocked_packet_types_.end()) {
    return false;
  }

  // There is no peer_order block for packet_data.from_node_id_ peer
  const auto peer_order_block = packet_peer_order_block->second.find(packet_data.from_node_id_);
  if (peer_order_block == packet_peer_order_block->second.end()) {
    return false;
  }

  // There is peer_order block for packet_data.type_ and packet_data.from_node_id_ peer
  // Block all packets that were received after peer_order block with lowest id (time)
  const auto smallest_blocking_packet_id = *peer_order_block->second.begin();
  if (packet_data.id_ > smallest_blocking_packet_id) {
    return true;
  }

  // This should never happen as packets id's are supposed to be unique
  assert(packet_data.id_ != smallest_blocking_packet_id);

  return false;
}

bool PacketsBlockingMask::isPacketBlocked(const PacketData& packet_data) const {
  // Check if packet is hard blocked
  if (isPacketHardBlocked(packet_data)) {
    return true;
  }

  // Custom blocks for specific packet types...
  // Check if NewDagBlockPacket is blocked by processing some dag blocks with <= dag level
  if (packet_data.type_ == SubprotocolPacketType::NewBlockPacket && isDagBlockPacketBlockedByLevel(packet_data)) {
    return true;
  }

  // Check if packet is order blocked
  if (isPacketPeerOrderBlocked(packet_data)) {
    return true;
  }

  return false;
}

}  // namespace taraxa::network::tarcap