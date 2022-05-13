#include "network/tarcap/packets_handler.hpp"

namespace taraxa::network::tarcap {

void PacketsHandler::registerHandler(SubprotocolPacketType packet_type, std::shared_ptr<PacketHandler> handler) {
  assert(packets_handlers_.find(packet_type) == packets_handlers_.end());

  packets_handlers_.emplace(packet_type, std::move(handler));
}

template <typename PacketHandlerType>
std::shared_ptr<PacketHandlerType>& PacketsHandler::getSpecificHandler(SubprotocolPacketType packet_type) {
  auto selected_handler = packets_handlers_.find(packet_type);

  if (selected_handler == packets_handlers_.end()) {
    assert(false);

    throw std::runtime_error("No registered packet handler for packet type: " + std::to_string(packet_type));
  }

  return std::static_pointer_cast<PacketHandlerType>(selected_handler->second);
}

}  // namespace taraxa::network::tarcap