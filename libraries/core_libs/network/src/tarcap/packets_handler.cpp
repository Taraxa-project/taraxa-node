#include "network/tarcap/packets_handler.hpp"

namespace taraxa::network::tarcap {

const std::shared_ptr<PacketHandler>& PacketsHandler::getSpecificHandler(SubprotocolPacketType packet_type) const {
  auto selected_handler = packets_handlers_.find(packet_type);

  if (selected_handler == packets_handlers_.end()) {
    assert(false);

    throw std::runtime_error("No registered packet handler for packet type: " + std::to_string(packet_type));
  }

  return selected_handler->second;
}

}  // namespace taraxa::network::tarcap