#include "packets_handler.hpp"

namespace taraxa::network::tarcap {

void PacketsHandler::registerHandler(PriorityQueuePacketType packet_type, std::shared_ptr<PacketHandler> handler) {
  assert(packets_handlers_.find(packet_type) == packets_handlers_.end());

  packets_handlers_.emplace(packet_type, std::move(handler));
}

std::shared_ptr<PacketHandler>& PacketsHandler::getSpecificHandler(PriorityQueuePacketType packet_type) {
  auto selected_handler = packets_handlers_.find(packet_type);

  if (selected_handler == packets_handlers_.end()) {
    assert(false);

    throw std::runtime_error("No registered packet handler for packet type: " + std::to_string(packet_type));
  }

  return selected_handler->second;
}

}  // namespace taraxa::network::tarcap