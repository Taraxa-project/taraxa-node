#include "packet_data.hpp"

namespace taraxa::network::tarcap {

PacketData::PacketData(PriorityQueuePacketType type, std::string&& type_str, dev::p2p::NodeID&& from_node_id_,
                       std::vector<unsigned char>&& bytes)
    : rlp_bytes_(std::move(bytes)),
      receive_time_(std::chrono::steady_clock::now()),
      type_(type),
      type_str_(std::move(type_str)),
      priority_(getPacketPriority(type)),
      from_node_id_(std::move(from_node_id_)),
      rlp_(dev::RLP(rlp_bytes_)) {}

/**
 * @param packet_type
 * @return PacketPriority <high/mid/low> based om packet_type
 */
PacketData::PacketPriority PacketData::getPacketPriority(PriorityQueuePacketType packet_type) {
  if (packet_type > PriorityQueuePacketType::kPqHighPriorityPackets &&
      packet_type < PriorityQueuePacketType::kPqMidPriorityPackets) {
    return PacketPriority::High;
  } else if (packet_type > PriorityQueuePacketType::kPqMidPriorityPackets &&
             packet_type < PriorityQueuePacketType::kPqLowPriorityPackets) {
    return PacketPriority::Mid;
  } else if (packet_type > PriorityQueuePacketType::kPqLowPriorityPackets) {
    return PacketPriority::Low;
  }

  assert(false);
}

}  // namespace taraxa::network::tarcap