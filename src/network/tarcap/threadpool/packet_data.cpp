#include "packet_data.hpp"

namespace taraxa::network::tarcap {

PacketData::PacketData(PriorityQueuePacketType type, std::string&& type_str, dev::p2p::NodeID&& from_node_id_,
                       std::vector<unsigned char>&& bytes)
    : receive_time_(std::chrono::steady_clock::now()),
      type_(type),
      type_str_(std::move(type_str)),
      priority_(getPacketPriority(type)),
      from_node_id_(std::move(from_node_id_)),
      rlp_bytes_(std::move(bytes)) {}

/**
 * @param packet_type
 * @return PacketPriority <high/mid/low> based om packet_type
 */
PacketData::PacketPriority PacketData::getPacketPriority(PriorityQueuePacketType packet_type) {
  if (packet_type > PriorityQueuePacketType::PQ_HighPriorityPackets &&
      packet_type < PriorityQueuePacketType::PQ_MidPriorityPackets) {
    return PacketPriority::High;
  } else if (packet_type > PriorityQueuePacketType::PQ_MidPriorityPackets &&
             packet_type < PriorityQueuePacketType::PQ_LowPriorityPackets) {
    return PacketPriority::Mid;
  } else if (packet_type > PriorityQueuePacketType::PQ_LowPriorityPackets) {
    return PacketPriority::Low;
  }

  assert(false);
}

}  // namespace taraxa::network::tarcap