#include "packet_handler.hpp"

namespace taraxa::network::tarcap {

PacketHandler::PacketHandler(std::shared_ptr<PeersState> peers_state, const addr_t& node_addr,
                             const std::string& log_channel_name)
    : peers_state_(std::move(peers_state)) {
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

    peer_ = peers_state_->getPeer(packet_data.from_node_id_);
    host_ = peers_state_->host_.lock();

    if (!peer_ || !host_) {
      return;
    }

    // Main processing function
    process(packet_data, dev::RLP(packet_data.rlp_bytes_));

    // TODO: maybe this should be set only when status packet received as now it should not be stuck in synchronous
    //       queue like before...
    // Any packet means that we are communicating so let's not disconnect
    peer_->setAlive();

    auto processing_duration =
        std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - begin);
    auto tp_wait_duration = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() -
                                                                                  packet_data.receive_time_);
    packet_stats.processing_duration_ = processing_duration;
    packet_stats.tp_wait_duration_ = tp_wait_duration;

    peers_state_->packets_stats_.addReceivedPacket(peers_state_->node_id_, convertPacketTypeToString(packet_data.type_),
                                                   packet_stats);

  } catch (...) {
    handle_read_exception(packet_data.from_node_id_, packet_data.type_);
  }
}

void PacketHandler::handle_read_exception(const dev::p2p::NodeID& node_id, SubprotocolPacketType packet_type) {
  try {
    throw;
  } catch (std::exception const& _e) {
    // TODO be more precise about the error handling
    LOG(log_er_) << "Read exception: " << _e.what() << ". PacketType: " << convertPacketTypeToString(packet_type)
                 << " (" << packet_type << ")";

    // TODO: get rid of host_ weak_ptr if possible ?
    peers_state_->host_.lock()->disconnect(node_id, dev::p2p::DisconnectReason::BadProtocol);
  }
}

}  // namespace taraxa::network::tarcap