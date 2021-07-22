#include "packet_data.hpp"

namespace taraxa::network::tarcap {

PacketData::PacketData(SubprotocolPacketType type, dev::p2p::NodeID&& from_node_id_, std::vector<unsigned char>&& bytes)
    : receive_time_(std::chrono::steady_clock::now()),
      type_(type),
      priority_(getPacketPriority(type)),
      from_node_id_(std::move(from_node_id_)),
      rlp_bytes_(std::move(bytes)) {}

/**
 * @param packet_type
 * @return PacketPriority <high/mid/low> based om packet_type
 */
PacketData::PacketPriority PacketData::getPacketPriority(SubprotocolPacketType packet_type) {
  if (packet_type > SubprotocolPacketType::HighPriorityPackets &&
      packet_type < SubprotocolPacketType::MidPriorityPackets) {
    return PacketPriority::High;
  } else if (packet_type > SubprotocolPacketType::MidPriorityPackets &&
             packet_type < SubprotocolPacketType::LowPriorityPackets) {
    return PacketPriority::Mid;
  } else if (packet_type > SubprotocolPacketType::LowPriorityPackets) {
    return PacketPriority::Low;
  }

  assert(false);
}

}  // namespace taraxa::network::tarcap