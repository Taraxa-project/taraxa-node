#pragma once

#include <libdevcore/RLP.h>

#include <memory>
#include <string_view>

#include "exceptions.hpp"
#include "network/tarcap/packet_types.hpp"
#include "network/tarcap/packets_handlers/latest/common/base_packet_handler.hpp"
#include "network/tarcap/packets_handlers/latest/common/exceptions.hpp"
#include "network/tarcap/shared_states/peers_state.hpp"
#include "network/tarcap/stats/time_period_packets_stats.hpp"
#include "network/tarcap/taraxa_peer.hpp"
#include "network/threadpool/packet_data.hpp"
#include "spdlogger/logging.hpp"

namespace taraxa::network::tarcap {

template <class PacketType>
PacketType decodePacketRlp(const dev::RLP& packet_rlp) {
  return util::rlp_dec<PacketType>(packet_rlp);
}

template <class PacketType>
dev::bytes encodePacketRlp(PacketType packet) {
  return util::rlp_enc(packet);
}

/**
 * @brief Packet handler base class that consists of shared state and some commonly used functions
 */
class PacketHandler : public BasePacketHandler {
 public:
  PacketHandler(const FullNodeConfig& conf, std::shared_ptr<PeersState> peers_state,
                std::shared_ptr<TimePeriodPacketsStats> packets_stats, const std::string& log_channel_name);

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
  // TODO: use unique_ptr for packet data for easier & quicker copying
  void processPacket(const threadpool::PacketData& packet_data) override;

 private:
  void handle_caught_exception(std::string_view exception_msg, const threadpool::PacketData& packet_data,
                               const dev::p2p::NodeID& peer,
                               dev::p2p::DisconnectReason disconnect_reason = dev::p2p::DisconnectReason::UserReason,
                               bool set_peer_as_malicious = false);

  /**
   * @brief Main packet processing function
   */
  virtual void process(const threadpool::PacketData& packet_data, const std::shared_ptr<TaraxaPeer>& peer) = 0;

 protected:
  bool sealAndSend(const dev::p2p::NodeID& node_id, SubprotocolPacketType packet_type, dev::bytes&& rlp_bytes);
  void disconnect(const dev::p2p::NodeID& node_id, dev::p2p::DisconnectReason reason);

 protected:
  // Node config
  const FullNodeConfig& kConf;

  std::shared_ptr<PeersState> peers_state_{nullptr};

  // Shared packet stats
  std::shared_ptr<TimePeriodPacketsStats> packets_stats_;

  // Declare logger instances
  spdlogger::Logger logger_;
};

}  // namespace taraxa::network::tarcap
