#pragma once

#include <memory>

#include "libdevcore/RLP.h"
#include "logger/logger.hpp"
#include "network/tarcap/packet_types.hpp"
#include "network/tarcap/packets_handlers/common/i_packet_handler.hpp"
#include "network/tarcap/shared_states/peers_state.hpp"
#include "network/tarcap/taraxa_peer.hpp"
#include "network/tarcap/threadpool/packet_data.hpp"

namespace taraxa::network::tarcap {

class PacketsStats;

// TODO: remove Templated
template <typename Packet>
class TemplatedPacketHandler : public IPacketHandler {
 public:
  virtual ~TemplatedPacketHandler() = default;
  TemplatedPacketHandler(std::shared_ptr<PeersState> peers_state, std::shared_ptr<PacketsStats> packets_stats,
                         const addr_t& node_addr, const std::string& log_channel_name);

  // TODO: rename PacketData to RawPacketData and packet_data to raw_packet_data
  void processPacket(const PacketData& raw_packet_data) override;

 private:
  /**
   * @brief Parses packed data (rlp)
   *
   * @param packet_data
   * @throws InvalidEncodingSize in case actual packet rlp items count != expected items count
   * @returns parsed Packet object
   */
  virtual Packet parsePacket(const PacketData& raw_packet_data);

  /**
   * @brief Main packet processing function
   *
   * @param packet
   * @param from_peer packet sender
   */
  virtual void process(Packet&& packet_data, const std::shared_ptr<TaraxaPeer>& from_peer) = 0;

 protected:
  bool sealAndSend(const dev::p2p::NodeID& nodeID, SubprotocolPacketType packet_type, dev::RLPStream&& rlp);
  void disconnect(dev::p2p::NodeID const& node_id, dev::p2p::DisconnectReason reason);

 protected:
  std::shared_ptr<PeersState> peers_state_{nullptr};

  // Shared packet stats
  std::shared_ptr<PacketsStats> packets_stats_;

  // Declare logger instances
  LOG_OBJECTS_DEFINE
};

template <typename Packet>
TemplatedPacketHandler<Packet>::TemplatedPacketHandler(std::shared_ptr<PeersState> peers_state,
                                                       std::shared_ptr<PacketsStats> packets_stats,
                                                       const addr_t& node_addr, const std::string& log_channel_name)
    : peers_state_(std::move(peers_state)), packets_stats_(std::move(packets_stats)) {
  LOG_OBJECTS_CREATE(log_channel_name);
}

template <typename Packet>
Packet TemplatedPacketHandler<Packet>::parsePacket(const PacketData& packet_data) {
  return util::encoding_rlp::rlp_dec<Packet>(util::RLPDecoderRef(packet_data.rlp_, true));
}

template <typename Packet>
void TemplatedPacketHandler<Packet>::processPacket(const PacketData& packet_data) {
  try {
    SinglePacketStats packet_stats{packet_data.from_node_id_, packet_data.rlp_.data().size(),
                                   std::chrono::microseconds(0), std::chrono::microseconds(0)};
    const auto begin = std::chrono::steady_clock::now();

    auto tmp_peer = peers_state_->getPeer(packet_data.from_node_id_);
    if (!tmp_peer && packet_data.type_ != SubprotocolPacketType::StatusPacket) {
      LOG(log_er_) << "Peer " << packet_data.from_node_id_.abridged()
                   << " not in peers map. He probably did not send initial status message - will be disconnected.";
      disconnect(packet_data.from_node_id_, dev::p2p::UserReason);
      return;
    }

    // Main processing function
    process(parsePacket(packet_data), tmp_peer);

    auto processing_duration =
        std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - begin);
    auto tp_wait_duration = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() -
                                                                                  packet_data.receive_time_);
    packet_stats.processing_duration_ = processing_duration;
    packet_stats.tp_wait_duration_ = tp_wait_duration;

    packets_stats_->addReceivedPacket(packet_data.type_str_, packet_stats);
  } catch (const std::exception& e) {
    // TODO: maybe we should not catch the exception here and disconnect peer ???
    LOG(log_er_) << "Processing packet " << packet_data.type_str_ << " (" << packet_data.type_
                 << ") exception: " << e.what();
    disconnect(packet_data.from_node_id_, dev::p2p::DisconnectReason::BadProtocol);
  } catch (...) {
    LOG(log_er_) << "Processing packet " << packet_data.type_str_ << " (" << packet_data.type_
                 << ") unknown exception.";
    disconnect(packet_data.from_node_id_, dev::p2p::DisconnectReason::BadProtocol);
  }
}

template <typename Packet>
bool TemplatedPacketHandler<Packet>::sealAndSend(const dev::p2p::NodeID& node_id, SubprotocolPacketType packet_type,
                                                 dev::RLPStream&& rlp) {
  auto host = peers_state_->host_.lock();
  if (!host) {
    LOG(log_er_) << "sealAndSend failed to obtain host";
    return false;
  }

  const auto [peer, is_pending] = peers_state_->getAnyPeer(node_id);
  if (!peer) [[unlikely]] {
    LOG(log_wr_) << "sealAndSend failed to find peer";
    return false;
  }

  if (is_pending && packet_type != SubprotocolPacketType::StatusPacket) [[unlikely]] {
    LOG(log_wr_) << "sealAndSend failed initial status check, peer " << node_id.abridged() << " will be disconnected";
    host->disconnect(node_id, dev::p2p::UserReason);
    return false;
  }

  const auto begin = std::chrono::steady_clock::now();
  const size_t packet_size = rlp.out().size();

  // TODO: use TARAXA_CAPABILITY_NAME instead of "taraxa"
  host->send(node_id, "taraxa", packet_type, rlp.invalidate(), [begin, node_id, packet_size, packet_type, this]() {
    SinglePacketStats packet_stats{
        node_id, packet_size,
        std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - begin),
        std::chrono::microseconds{0}};
    packets_stats_->addSentPacket(convertPacketTypeToString(packet_type), packet_stats);
  });

  return true;
}

template <typename Packet>
void TemplatedPacketHandler<Packet>::disconnect(dev::p2p::NodeID const& node_id, dev::p2p::DisconnectReason reason) {
  if (auto host = peers_state_->host_.lock(); host) {
    host->disconnect(node_id, reason);
  } else {
    LOG(log_wr_) << "Invalid host " << node_id.abridged();
  }
}

}  // namespace taraxa::network::tarcap
