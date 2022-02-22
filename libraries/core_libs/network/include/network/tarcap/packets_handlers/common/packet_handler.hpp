#pragma once

#include <memory>

#include "libdevcore/RLP.h"
#include "logger/logger.hpp"
#include "network/tarcap/packet_types.hpp"
#include "network/tarcap/packets_handlers/common/exceptions.hpp"
#include "network/tarcap/shared_states/peers_state.hpp"
#include "network/tarcap/taraxa_peer.hpp"
#include "network/tarcap/threadpool/packet_data.hpp"

namespace taraxa::network::tarcap {

// Taraxa capability name
constexpr char TARAXA_CAPABILITY_NAME[] = "taraxa";

class PacketsStats;

/**
 * @brief Packet handler base class that consists of shared state and some commonly used functions
 */
class PacketHandler {
 public:
  PacketHandler(std::shared_ptr<PeersState> peers_state, std::shared_ptr<PacketsStats> packets_stats,
                const addr_t& node_addr, const std::string& log_channel_name);
  virtual ~PacketHandler() = default;

  /**
   * @brief Packet processing function wrapper that logs packet stats and calls process function
   *
   * @param packet_data
   */
  void processPacket(const PacketData& packet_data);

 private:
  void handle_caught_exception(const char* exception_msg, const PacketData& packet_data,
                               dev::p2p::DisconnectReason disconnect_reason = dev::p2p::DisconnectReason::UserReason,
                               bool set_peer_as_malicious = false);

  /**
   * @brief Main packet processing function
   */
  virtual void process(const PacketData& packet_data, const std::shared_ptr<TaraxaPeer>& peer) = 0;

  /**
   * @brief Validates packet rlp format - items count
   *
   * @throws InvalidRlpItemsCountException exception
   */
  virtual void validatePacketRlpFormat(const PacketData& packet_data) = 0;

 protected:
  /**
   * @brief Checks if packet rlp is a list, if not it throws InvalidRlpItemsCountException
   *
   * @param packet_data
   * @throws InvalidRlpItemsCountException exception
   */
  void checkPacketRlpList(const PacketData& packet_data);

  bool sealAndSend(const dev::p2p::NodeID& nodeID, SubprotocolPacketType packet_type, dev::RLPStream&& rlp);
  void disconnect(dev::p2p::NodeID const& node_id, dev::p2p::DisconnectReason reason);

 protected:
  std::shared_ptr<PeersState> peers_state_{nullptr};

  // Shared packet stats
  std::shared_ptr<PacketsStats> packets_stats_;

  // Declare logger instances
  LOG_OBJECTS_DEFINE
};

}  // namespace taraxa::network::tarcap
