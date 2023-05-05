#pragma once

#include <memory>
#include <unordered_map>

#include "network/tarcap/packets_handlers/latest/common/packet_handler.hpp"

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
   * @brief templated getSpecificHandler method for getting specific packet handler based on
   * PacketHandlerType::kPacketType_
   * @tparam PacketHandlerType
   *
   * @return std::shared_ptr<PacketHandlerType>
   */
  template <typename PacketHandlerType>
  std::shared_ptr<PacketHandlerType> getSpecificHandler();

  /**
   * @brief Registers packet handler
   *
   * @tparam PacketHandlerType
   * @tparam Args
   * @param args to be forwarded to the PacketHandlerType ctor
   */
  template <typename PacketHandlerType, typename... Args>
  void registerHandler(Args&&... args);

 private:
  // Map of all packets handlers, factory method selects specific packet handler for processing based on packet type
  std::unordered_map<SubprotocolPacketType, std::shared_ptr<PacketHandler>> packets_handlers_;
};

template <typename PacketHandlerType>
std::shared_ptr<PacketHandlerType> PacketsHandler::getSpecificHandler() {
  return std::static_pointer_cast<PacketHandlerType>(getSpecificHandler(PacketHandlerType::kPacketType_));
}

template <typename PacketHandlerType, typename... Args>
void PacketsHandler::registerHandler(Args&&... args) {
  assert(packets_handlers_.find(PacketHandlerType::kPacketType_) == packets_handlers_.end());
  packets_handlers_.emplace(PacketHandlerType::kPacketType_,
                            std::make_shared<PacketHandlerType>(std::forward<Args>(args)...));
}

}  // namespace taraxa::network::tarcap
