#pragma once

#include "network/threadpool/packet_data.hpp"

namespace taraxa::network::tarcap {

/**
 * @brief Base Packet handler base class that consists processPacket function
 */
class BasePacketHandler {
 public:
  BasePacketHandler() = default;
  virtual ~BasePacketHandler() = default;
  BasePacketHandler(const BasePacketHandler&) = default;
  BasePacketHandler(BasePacketHandler&&) = default;
  BasePacketHandler& operator=(const BasePacketHandler&) = default;
  BasePacketHandler& operator=(BasePacketHandler&&) = default;

  /**
   * @brief Packet processing function wrapper
   *
   * @param packet_data
   */
  // TODO: use unique_ptr for packet data for easier & quicker copying
  virtual void processPacket(const threadpool::PacketData& packet_data) = 0;
};

}  // namespace taraxa::network::tarcap
