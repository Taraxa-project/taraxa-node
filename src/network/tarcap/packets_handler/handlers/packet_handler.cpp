#include "packet_handler.hpp"

namespace taraxa::network::tarcap {

PacketHandler::PacketHandler(std::shared_ptr<PeersState> peers_state, const addr_t& node_addr,
                             const std::string& log_channel_name)
    : peers_state_(std::move(peers_state)) {
  LOG_OBJECTS_CREATE(log_channel_name);
}

void PacketHandler::processPacket(const PacketData& packet_data) {
  try {
    PacketStats packet_stats{packet_data.from_node_id_, packet_data.rlp_bytes_.size(), false,
                             std::chrono::microseconds(0)};
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
    // queue
    //       like before...
    // Any packet means that we are comunicating so let's not disconnect
    // peer->setAlive();

    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - begin);
    packet_stats.total_duration_ = duration;

    received_packets_stats_->addPacket(convertPacketTypeToString(packet_data.type_), packet_stats);

    // TODO: every handler creates his own logger so this net_per logs must be handled somehow differently
    //    LOG(log_dg_net_per_) << "(\"" << shared_state_->node_id_ << "\") received " << packetTypeToString(_id) << "
    //    packet from (\""
    //                         << packet_data.from_node_id << "\"). Stats: " << packet_stats;
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

    // TODO: get rid of host_ weak_ptr
    peers_state_->host_.lock()->disconnect(node_id, dev::p2p::DisconnectReason::BadProtocol);
  }
}

}  // namespace taraxa::network::tarcap