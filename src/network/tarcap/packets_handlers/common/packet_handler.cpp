#include "packet_handler.hpp"

#include "network/tarcap/stats/packets_stats.hpp"

namespace taraxa::network::tarcap {

PacketHandler::PacketHandler(std::shared_ptr<PeersState> peers_state, std::shared_ptr<PacketsStats> packets_stats,
                             const addr_t& node_addr, const std::string& log_channel_name)
    : peers_state_(std::move(peers_state)), packets_stats_(std::move(packets_stats)) {
  LOG_OBJECTS_CREATE(log_channel_name);
}

void PacketHandler::processPacket(const PacketData& packet_data) {
  std::shared_ptr<dev::p2p::Host> tmp_host{nullptr};

  try {
    SinglePacketStats packet_stats{packet_data.from_node_id_, packet_data.rlp_bytes_.size(), false,
                                   std::chrono::microseconds(0), std::chrono::microseconds(0)};
    auto begin = std::chrono::steady_clock::now();

    tmp_host = peers_state_->host_.lock();
    if (!tmp_host) {
      LOG(log_er_) << "Invalid host during packet processing";
      return;
    }

    auto tmp_peer = peers_state_->getPeer(packet_data.from_node_id_);
    if (!tmp_peer && packet_data.type_ != PriorityQueuePacketType::PQ_StatusPacket) {
      LOG(log_er_) << "Peer " << packet_data.from_node_id_.abridged()
                   << " not in peers map. He probably did not send initial status message - will be disconnected.";
      tmp_host->disconnect(packet_data.from_node_id_, dev::p2p::UserReason);
      return;
    }

    // Main processing function
    process(dev::RLP(packet_data.rlp_bytes_), packet_data, tmp_host, tmp_peer);

    auto processing_duration =
        std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - begin);
    auto tp_wait_duration = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() -
                                                                                  packet_data.receive_time_);
    packet_stats.processing_duration_ = processing_duration;
    packet_stats.tp_wait_duration_ = tp_wait_duration;

    packets_stats_->addReceivedPacket(peers_state_->node_id_, packet_data.type_str_, packet_stats);

  } catch (...) {
    handle_read_exception(tmp_host, packet_data);
  }
}

void PacketHandler::handle_read_exception(const std::shared_ptr<dev::p2p::Host>& host, const PacketData& packet_data) {
  try {
    throw;
  } catch (std::exception const& _e) {
    LOG(log_er_) << "Read exception: " << _e.what() << ". PacketType: " << packet_data.type_str_ << " ("
                 << packet_data.type_ << ")";

    host->disconnect(packet_data.from_node_id_, dev::p2p::DisconnectReason::BadProtocol);
  }
}

bool PacketHandler::sealAndSend(const dev::p2p::NodeID& nodeID, SubprotocolPacketType packet_type, dev::RLPStream rlp) {
  auto host = peers_state_->host_.lock();
  if (!host) {
    LOG(log_er_) << "sealAndSend failed to obtain host";
    return false;
  }

  auto peer = peers_state_->getPeer(nodeID);
  if (!peer) {
    peer = peers_state_->getPendingPeer(nodeID);

    if (!peer) {
      LOG(log_er_) << "sealAndSend failed to find peer";
      return false;
    }

    if (packet_type != SubprotocolPacketType::StatusPacket) {
      LOG(log_er_) << "sealAndSend failed initial status check, peer " << nodeID.abridged() << " will be disconnected";
      host->disconnect(nodeID, dev::p2p::UserReason);
      return false;
    }
  }

  auto packet_size = rlp.out().size();

  // This situation should never happen - packets bigger than 16MB cannot be sent due to networking layer limitations.
  // If we are trying to send packets bigger than that, it should be split to multiple packets
  // or handled in some other way in high level code - e.g. function that creates such packet and calls sealAndSend
  if (packet_size > MAX_PACKET_SIZE) {
    LOG(log_er_) << "Trying to send packet bigger than PACKET_MAX_SIZE(" << MAX_PACKET_SIZE << ") - rejected !"
                 << " Packet type: " << convertPacketTypeToString(packet_type) << ", size: " << packet_size
                 << ", receiver: " << nodeID.abridged();
    return false;
  }

  host->send(nodeID, TARAXA_CAPABILITY_NAME, packet_type, move(rlp.invalidate()));

  SinglePacketStats packet_stats{nodeID, packet_size, false, std::chrono::microseconds{0},
                                 std::chrono::microseconds{0}};
  packets_stats_->addSentPacket(peers_state_->node_id_, convertPacketTypeToString(packet_type), packet_stats);

  return true;
}

}  // namespace taraxa::network::tarcap