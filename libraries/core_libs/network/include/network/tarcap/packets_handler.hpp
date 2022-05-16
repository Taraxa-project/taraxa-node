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
  const std::shared_ptr<PacketHandler>& getSpecificHandler(SubprotocolPacketType packet_type) const;

  /**
   * @brief templated getSpecificHandler method, which internally casts fetched PacketHandler to specified
   * PacketHandlerType
   * @see non-templated getSpecificHandler method for other specs
   */
  template <typename PacketHandlerType>
  std::shared_ptr<PacketHandlerType> getSpecificHandler(SubprotocolPacketType packet_type);

  /**
   * @brief Registers handler for specific packet type
   *
   * @param packet_type
   * @param handler
   */
  void registerHandler(SubprotocolPacketType packet_type, std::shared_ptr<PacketHandler> handler);

 private:
  // Map of all packets handlers, factory method selects specific packet handler for processing based on packet type
  std::unordered_map<SubprotocolPacketType, std::shared_ptr<PacketHandler>> packets_handlers_;
};

template <typename PacketHandlerType>
std::shared_ptr<PacketHandlerType> PacketsHandler::getSpecificHandler(SubprotocolPacketType packet_type) {
  return std::static_pointer_cast<PacketHandlerType>(getSpecificHandler(packet_type));
}

}  // namespace taraxa::network::tarcap
