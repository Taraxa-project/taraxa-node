#pragma once

#include <libp2p/Common.h>

#include <chrono>

#include "json/value.h"
#include "network/tarcap/packet_types.hpp"

namespace taraxa::network::tarcap {

class PacketData {
 public:
  using PacketId = uint64_t;
  enum PacketPriority : size_t { High = 0, Mid, Low, Count };

  PacketData(SubprotocolPacketType type, dev::p2p::NodeID&& from_node_id_, std::vector<unsigned char>&& bytes);
  PacketData(const PacketData&) = default;
  PacketData(PacketData&&) = default;
  PacketData& operator=(const PacketData&) = default;
  PacketData& operator=(PacketData&&) = default;

  /**
   * @return PacketData json representation
   */
  Json::Value getPacketDataJson() const;

 private:
  /**
   * @param packet_type
   * @return PacketPriority <high/mid/low> based om packet_type
   */
  static inline PacketPriority getPacketPriority(SubprotocolPacketType packet_type);

  // Packet bytes had to be copied here as dev::RLP does not own vector of bytes, it only "points to" it
  std::vector<unsigned char> rlp_bytes_;

 public:
  PacketId id_{0};  // Unique packet id (counter)
  std::chrono::steady_clock::time_point receive_time_;
  SubprotocolPacketType type_;
  std::string type_str_;
  PacketPriority priority_;
  dev::p2p::NodeID from_node_id_;
  dev::RLP rlp_;
};

}  // namespace taraxa::network::tarcap