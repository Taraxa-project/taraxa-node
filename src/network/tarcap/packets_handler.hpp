#pragma once

#include <memory>
#include <unordered_map>

#include "network/tarcap/packets_handlers/common/packet_handler.hpp"

namespace taraxa::network::tarcap {

/**
 * @brief Generic PacketsHandler that contains all specific packet handlers
 */
class PacketsHandler {
 public:
  /**
   * @brief Factory method for getting specific packet handler based on packet type
   * @note reference to pointer is returned, it is user's responsibility to decide if he makes a copy to independently
   *       control the lifetime of the object or not.
   *
   * @param packet_type
   * @return reference to std::shared_ptr<PacketsHandler>
   */
  std::shared_ptr<PacketHandler>& getSpecificHandler(PriorityQueuePacketType packet_type);

  /**
   * @brief Registers handler for specific packet type
   *
   * @param packet_type
   * @param handler
   */
  void registerHandler(PriorityQueuePacketType packet_type, std::shared_ptr<PacketHandler> handler);

 private:
  // Map of all packets handlers, factory method selects specific packet handler for processing based on packet type
  std::unordered_map<PriorityQueuePacketType, std::shared_ptr<PacketHandler>> packets_handlers_;
};

}  // namespace taraxa::network::tarcap
