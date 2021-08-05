#pragma once

#include <libp2p/Common.h>

#include <chrono>

#include "network/tarcap/packet_types.hpp"

namespace taraxa::network::tarcap {

class PacketData {
 public:
  enum PacketPriority : size_t { High = 0, Mid, Low, Count };

  /**
   * @param packet_type
   * @return PacketPriority <high/mid/low> based om packet_type
   */
  static inline PacketPriority getPacketPriority(PriorityQueuePacketType packet_type);

  PacketData(PriorityQueuePacketType type, std::string&& type_str, dev::p2p::NodeID&& from_node_id_, std::vector<unsigned char>&& bytes);

 public:
  std::chrono::steady_clock::time_point receive_time_;
  PriorityQueuePacketType type_;
  std::string type_str_;
  PacketPriority priority_;
  dev::p2p::NodeID from_node_id_;
  std::vector<unsigned char> rlp_bytes_;
};

}  // namespace taraxa::network::tarcap