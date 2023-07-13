#pragma once

#include <libdevcore/RLP.h>

#include <memory>
#include <string_view>

#include "common/thread_pool.hpp"
#include "exceptions.hpp"
#include "logger/logger.hpp"
#include "network/tarcap/packet_types.hpp"
#include "network/tarcap/shared_states/peers_state.hpp"
#include "network/tarcap/taraxa_peer.hpp"
#include "network/threadpool/packet_data.hpp"

namespace taraxa::network::tarcap {

// Taraxa capability name
constexpr char TARAXA_CAPABILITY_NAME[] = "taraxa";

class TimePeriodPacketsStats;

/**
 * @brief Packet handler base class that consists of shared state and some commonly used functions
 */
class PacketHandler {
 public:
  PacketHandler(const FullNodeConfig& conf, std::shared_ptr<PeersState> peers_state,
                std::shared_ptr<TimePeriodPacketsStats> packets_stats, const addr_t& node_addr,
                const std::string& log_channel_name);
  virtual ~PacketHandler() = default;
  PacketHandler(const PacketHandler&) = default;
  PacketHandler(PacketHandler&&) = default;
  PacketHandler& operator=(const PacketHandler&) = delete;
  PacketHandler& operator=(PacketHandler&&) = delete;

  /**
   * @brief Packet processing function wrapper that logs packet stats and calls process function
   *
   * @param packet_data
   */
  void processPacket(const threadpool::PacketData& packet_data);

  void requestPbftNextVotesAtPeriodRound(const dev::p2p::NodeID& peerID, PbftPeriod pbft_period, PbftRound pbft_round);

 private:
  void handle_caught_exception(std::string_view exception_msg, const threadpool::PacketData& packet_data,
                               const dev::p2p::NodeID& peer,
                               dev::p2p::DisconnectReason disconnect_reason = dev::p2p::DisconnectReason::UserReason,
                               bool set_peer_as_malicious = false);

  /**
   * @brief Main packet processing function
   */
  virtual void process(const threadpool::PacketData& packet_data, const std::shared_ptr<TaraxaPeer>& peer) = 0;

  /**
   * @brief Validates packet rlp format - items count
   *
   * @throws InvalidRlpItemsCountException exception
   */
  virtual void validatePacketRlpFormat(const threadpool::PacketData& packet_data) const = 0;

 protected:
  /**
   * @brief Checks if packet rlp is a list, if not it throws InvalidRlpItemsCountException
   *
   * @param packet_data
   * @throws InvalidRlpItemsCountException exception
   */
  void checkPacketRlpIsList(const threadpool::PacketData& packet_data) const;

  bool sealAndSend(const dev::p2p::NodeID& nodeID, SubprotocolPacketType packet_type, dev::RLPStream&& rlp);
  void disconnect(const dev::p2p::NodeID& node_id, dev::p2p::DisconnectReason reason);

 protected:
  // Node config
  const FullNodeConfig& kConf;

  std::shared_ptr<PeersState> peers_state_{nullptr};

  // Shared packet stats
  std::shared_ptr<TimePeriodPacketsStats> packets_stats_;

  // Declare logger instances
  LOG_OBJECTS_DEFINE
};

}  // namespace taraxa::network::tarcap
