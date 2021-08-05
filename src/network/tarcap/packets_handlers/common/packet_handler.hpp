#pragma once

#include <memory>

#include "libdevcore/RLP.h"
#include "logger/log.hpp"
#include "network/tarcap/packet_types.hpp"
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
  // TODO: node_addr is everywhere just because it is used (hardcoded) inside LOG_OBJECTS_CREATE macro - refactor this !
  PacketHandler(std::shared_ptr<PeersState> peers_state, std::shared_ptr<PacketsStats> packets_stats,
                const addr_t& node_addr, const std::string& log_channel_name);

  /**
   * @brief Packet processing function wrapper that logs packet stats and calls process function
   *
   * @param packet_data
   */
  void processPacket(const PacketData& packet_data);

  std::string getCapabilityName() const;

 private:
  void handle_read_exception(const dev::p2p::NodeID& node_id, const PacketData& packet_data);

  /**
   * @brief Main packet processing function
   * @note packet_rlp is RLP object created from packet_data.rlp_bytes
   */
  virtual void process(const PacketData& packet_data, const dev::RLP& packet_rlp) = 0;

  /**
   * @brief Initializes tmp_* variables that at the beginning of processPacket function
   *
   * @param from_node_id node id of peer we received the packet from
   */
  void initTmpVariables(const dev::p2p::NodeID& from_node_id);

  /**
   * @brief Resets tmp_* variables that are set at the beginning of processPacket function
   */
  void resetTmpVariables();

 protected:
  static constexpr uint32_t MAX_PACKET_SIZE = 15 * 1024 * 1024;  // 15 MB -> 15 * 1024 * 1024 B

  // TODO: why dev::RLPStream and not const & or && ???
  bool sealAndSend(const dev::p2p::NodeID& nodeID, SubprotocolPacketType packet_type, dev::RLPStream rlp);

 protected:
  std::shared_ptr<PeersState> peers_state_{nullptr};

  // tmp variables that are initialized in every processPacket() call and should be valid also for process() call
  std::shared_ptr<TaraxaPeer> tmp_peer_{nullptr};
  std::shared_ptr<dev::p2p::Host> tmp_host_{nullptr};

  // Shared packet stats
  std::shared_ptr<PacketsStats> packets_stats_;

  // Declare logger instances
  LOG_OBJECTS_DEFINE
};

}  // namespace taraxa::network::tarcap
