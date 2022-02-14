#pragma once

#include "network/tarcap/threadpool/packet_data.hpp"

namespace taraxa::network::tarcap {

/**
 * @brief PacketHandler interface (abstract class) used in tarcap_threadpool
 */
class IPacketHandler {
 public:
  virtual ~IPacketHandler() = default;

  /**
   * @brief Main packet processing function
   *
   * @param packet_data
   */
  virtual void processPacket(const PacketData& packet_data) = 0;
};

}  // namespace taraxa::network::tarcap