#include "network/tarcap/threadpool/packet_data.hpp"

namespace taraxa::network::tarcap {

PacketData::PacketData(SubprotocolPacketType type, dev::p2p::NodeID&& from_node_id_, std::vector<unsigned char>&& bytes)
    : rlp_bytes_(std::move(bytes)),
      receive_time_(std::chrono::steady_clock::now()),
      type_(type),
      type_str_(convertPacketTypeToString(static_cast<SubprotocolPacketType>(type_))),
      priority_(getPacketPriority(type)),
      from_node_id_(std::move(from_node_id_)),
      rlp_(dev::RLP(rlp_bytes_)) {}

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

Json::Value PacketData::getPacketDataJson() const {
  Json::Value ret;

  // Transforms receive_time into the human-readable form
  const std::time_t t_c = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now() +
                                                               (receive_time_ - std::chrono::steady_clock::now()));
  std::ostringstream receive_time_os;
  receive_time_os << std::put_time(std::localtime(&t_c), "%F %T node local time");

  ret["id"] = Json::UInt64(id_);
  ret["type"] = type_str_;
  ret["priority"] = Json::UInt64(priority_);
  ret["receive_time"] = receive_time_os.str();
  ret["from_node_id"] = from_node_id_.toString();
  ret["rlp_items_count"] = rlp_.itemCount();
  ret["rlp_size"] = rlp_.size();

  return ret;
}

}  // namespace taraxa::network::tarcap