#include "packet_handler.hpp"

#include "network/tarcap/stats/packets_stats.hpp"

namespace taraxa::network::tarcap {

PacketHandler::PacketHandler(std::shared_ptr<PeersState> peers_state, std::shared_ptr<PacketsStats> packets_stats,
                             const addr_t& node_addr, const std::string& log_channel_name)
    : peers_state_(std::move(peers_state)), packets_stats_(std::move(packets_stats)) {
  LOG_OBJECTS_CREATE(log_channel_name);
}

void PacketHandler::processPacket(const PacketData& packet_data) {
  try {
    SinglePacketStats packet_stats{packet_data.from_node_id_, packet_data.rlp_bytes_.size(), false,
                                   std::chrono::microseconds(0), std::chrono::microseconds(0)};
    auto begin = std::chrono::steady_clock::now();

    // TODO: do we even use this ???
    //  // Delay is used only when we want to simulate some network delay
    //  uint64_t delay = conf_.network_simulated_delay ? getSimulatedNetworkDelay(_r, _nodeID) : 0;

    // Inits tmp_peer_ and tmp_host_
    initTmpVariables(packet_data.from_node_id_);

    if (!tmp_peer_) {
      LOG(log_er_) << "Invalid peer during packet processing";
      return;
    }

    if (!tmp_host_) {
      LOG(log_er_) << "Invalid host during packet processing";
      return;
    }

    // Main processing function
    process(packet_data, dev::RLP(packet_data.rlp_bytes_));

    // TODO: maybe this should be set only when status packet received as now it should not be stuck in synchronous
    //       queue like before...
    // Any packet means that we are communicating so let's not disconnect
    tmp_peer_->setAlive();

    auto processing_duration =
        std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - begin);
    auto tp_wait_duration = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() -
                                                                                  packet_data.receive_time_);
    packet_stats.processing_duration_ = processing_duration;
    packet_stats.tp_wait_duration_ = tp_wait_duration;

    packets_stats_->addReceivedPacket(peers_state_->node_id_, packet_data.type_str_,
                                      packet_stats);

    // Resets tmp_peer_ and tmp_host_
    resetTmpVariables();
  } catch (...) {
    handle_read_exception(packet_data.from_node_id_, packet_data);

    // Resets tmp_peer_ and tmp_host_
    resetTmpVariables();
  }
}

void PacketHandler::initTmpVariables(const dev::p2p::NodeID& from_node_id) {
  tmp_peer_ = peers_state_->getPeer(from_node_id);
  tmp_host_ = peers_state_->host_.lock();
}

void PacketHandler::resetTmpVariables() {
  tmp_peer_ = nullptr;
  tmp_host_ = nullptr;
}

void PacketHandler::handle_read_exception(const dev::p2p::NodeID& node_id, const PacketData& packet_data) {
  try {
    throw;
  } catch (std::exception const& _e) {
    LOG(log_er_) << "Read exception: " << _e.what() << ". PacketType: " << packet_data.type_str_
                 << " (" << packet_data.type_ << ")";

    assert(tmp_host_ != nullptr);
    tmp_host_->disconnect(node_id, dev::p2p::DisconnectReason::BadProtocol);
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