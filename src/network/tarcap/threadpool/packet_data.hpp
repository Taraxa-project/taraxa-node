#pragma once

#include <libp2p/Common.h>

#include "network/tarcap/packet_types.hpp"

namespace taraxa::network::tarcap {

class PacketData {
 public:
  enum PacketPriority : size_t { High = 0, Mid, Low, Count };

  /**
   * @param packet_type
   * @return PacketPriority <high/mid/low> based om packet_type
   */
  static inline PacketPriority getPacketPriority(SubprotocolPacketType packet_type);

  PacketData(SubprotocolPacketType type, dev::p2p::NodeID&& from_node_id_, std::vector<unsigned char>&& bytes);

 public:
  SubprotocolPacketType type_;
  PacketPriority priority_;
  dev::p2p::NodeID from_node_id_;
  std::vector<unsigned char> rlp_bytes_;
};

}  // namespace taraxa::network::tarcap