#pragma once

#include <memory>

#include "libdevcore/RLP.h"
#include "logger/log.hpp"
#include "network/tarcap/packet_types.hpp"
#include "network/tarcap/packets_handler/peers_state.hpp"
#include "network/tarcap/threadpool/packet_data.hpp"

namespace taraxa::network::tarcap {

/**
 * @brief Packet handler base class that consists of shared state and some commonly used functions
 */
class PacketHandler {
 public:
  // TODO: node_addr is everywhere just because it is used (hardcoded) inside LOG_OBJECTS_CREATE macro - refactor this !
  PacketHandler(std::shared_ptr<PeersState> peers_state, const addr_t& node_addr, const std::string& log_channel_name);

  /**
   * @brief Packet processing function wrapper that logs packet stats and calls process function
   *
   * @param packet_data
   */
  void processPacket(const PacketData& packet_data);
  void handle_read_exception(const dev::p2p::NodeID& node_id, SubprotocolPacketType packet_type);

 private:
  /**
   * @brief Main packet processing function
   * @note packet_rlp is RLP object created from packet_data.rlp_bytes
   */
  virtual void process(const PacketData& packet_data, const dev::RLP& packet_rlp) = 0;

 protected:
  std::shared_ptr<PeersState> peers_state_{nullptr};

  // tmp variables that are initialized in every processPacket() call and should be valid also for process() call
  std::shared_ptr<TaraxaPeer> peer_{nullptr};
  std::shared_ptr<dev::p2p::Host> host_{nullptr};

  // Declare logger instances
  LOG_OBJECTS_DEFINE
};

}  // namespace taraxa::network::tarcap
