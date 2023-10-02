#include "network/tarcap/packets_handlers/latest/common/packet_handler.hpp"

#include "network/tarcap/stats/time_period_packets_stats.hpp"

namespace taraxa::network::tarcap {

PacketHandler::PacketHandler(const FullNodeConfig& conf, std::shared_ptr<PeersState> peers_state,
                             std::shared_ptr<TimePeriodPacketsStats> packets_stats, const addr_t& node_addr,
                             const std::string& log_channel_name)
    : kConf(conf), peers_state_(std::move(peers_state)), packets_stats_(std::move(packets_stats)) {
  LOG_OBJECTS_CREATE(log_channel_name);
}

void PacketHandler::checkPacketRlpIsList(const threadpool::PacketData& packet_data) const {
  if (!packet_data.rlp_.isList()) {
    throw InvalidRlpItemsCountException(packet_data.type_str_ + " RLP must be a list. ", 0, 1);
  }
}

void PacketHandler::processPacket(const threadpool::PacketData& packet_data) {
  try {
    const auto begin = std::chrono::steady_clock::now();

    // It can rarely happen that packet was received and pushed into the queue when peer was still in peers map,
    // in the meantime the connection was lost and we started to process packet from such peer
    const auto peer = peers_state_->getPacketSenderPeer(packet_data.from_node_id_, packet_data.type_);
    if (!peer.first) [[unlikely]] {
      LOG(log_wr_) << "Unable to process packet. Reason: " << peer.second;
      disconnect(packet_data.from_node_id_, dev::p2p::UserReason);
      return;
    }

    // Validates packet rlp format
    // In case there is a type mismatch, one of the dev::RLPException's is thrown during further parsing in process
    // function
    checkPacketRlpIsList(packet_data);
    validatePacketRlpFormat(packet_data);

    // Main processing function
    process(packet_data, peer.first);

    auto processing_duration =
        std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - begin);
    auto tp_wait_duration = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() -
                                                                                  packet_data.receive_time_);

    PacketStats packet_stats{1 /* count */, packet_data.rlp_.data().size(), processing_duration, tp_wait_duration};
    peer.first->addSentPacket(packet_data.type_str_, packet_stats);

    if (kConf.network.ddos_protection.log_packets_stats) {
      packets_stats_->addReceivedPacket(packet_data.type_str_, packet_data.from_node_id_, packet_stats);
    }

  } catch (const MaliciousPeerException& e) {
    // thrown during packets processing -> malicious peer, invalid rlp items count, ...
    // If there is custom peer set in exception, disconnect him, not packet sender
    if (const auto custom_peer = e.getPeer(); custom_peer.has_value()) {
      handle_caught_exception(e.what(), packet_data, *custom_peer, e.getDisconnectReason(),
                              true /* set peer as malicious */);
    } else {
      handle_caught_exception(e.what(), packet_data, packet_data.from_node_id_, e.getDisconnectReason(),
                              true /* set peer as malicious */);
    }
  } catch (const PacketProcessingException& e) {
    // thrown during packets processing...
    handle_caught_exception(e.what(), packet_data, packet_data.from_node_id_, e.getDisconnectReason(),
                            true /* set peer as malicious */);
  } catch (const dev::RLPException& e) {
    // thrown during parsing inside aleth/libdevcore -> type mismatch
    handle_caught_exception(e.what(), packet_data, packet_data.from_node_id_, dev::p2p::DisconnectReason::BadProtocol,
                            true /* set peer as malicious */);
  } catch (const std::exception& e) {
    handle_caught_exception(e.what(), packet_data, packet_data.from_node_id_);
  } catch (...) {
    handle_caught_exception("Unknown exception", packet_data, packet_data.from_node_id_);
  }
}

void PacketHandler::handle_caught_exception(std::string_view exception_msg, const threadpool::PacketData& packet_data,
                                            const dev::p2p::NodeID& peer, dev::p2p::DisconnectReason disconnect_reason,
                                            bool set_peer_as_malicious) {
  LOG(log_er_) << "Exception caught during packet processing: " << exception_msg << " ."
               << "PacketData: " << jsonToUnstyledString(packet_data.getPacketDataJson())
               << ", disconnect peer: " << peer.toString();

  if (set_peer_as_malicious) {
    peers_state_->set_peer_malicious(peer);
  }

  disconnect(peer, disconnect_reason);
}

bool PacketHandler::sealAndSend(const dev::p2p::NodeID& node_id, SubprotocolPacketType packet_type,
                                dev::RLPStream&& rlp) {
  auto host = peers_state_->host_.lock();
  if (!host) {
    LOG(log_er_) << "sealAndSend failed to obtain host";
    return false;
  }

  if (const auto peer = peers_state_->getPacketSenderPeer(node_id, packet_type); !peer.first) [[unlikely]] {
    LOG(log_wr_) << "Unable to send packet. Reason: " << peer.second;
    host->disconnect(node_id, dev::p2p::UserReason);
    return false;
  }

  const auto begin = std::chrono::steady_clock::now();
  const size_t packet_size = rlp.out().size();

  host->send(node_id, TARAXA_CAPABILITY_NAME, packet_type, rlp.invalidate(),
             [begin, node_id, packet_size, packet_type, this]() {
               if (!kConf.network.ddos_protection.log_packets_stats) {
                 return;
               }

               PacketStats packet_stats{
                   1 /* count */, packet_size,
                   std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - begin),
                   std::chrono::microseconds{0}};

               packets_stats_->addSentPacket(convertPacketTypeToString(packet_type), node_id, packet_stats);
             });

  return true;
}

void PacketHandler::disconnect(const dev::p2p::NodeID& node_id, dev::p2p::DisconnectReason reason) {
  if (auto host = peers_state_->host_.lock(); host) {
    LOG(log_nf_) << "Disconnect node " << node_id.abridged();
    host->disconnect(node_id, reason);
  } else {
    LOG(log_er_) << "Unable to disconnect node " << node_id.abridged() << " due to invalid host.";
  }
}

void PacketHandler::requestPbftNextVotesAtPeriodRound(const dev::p2p::NodeID& peerID, PbftPeriod pbft_period,
                                                      PbftRound pbft_round) {
  LOG(log_dg_) << "Sending GetNextVotesSyncPacket with period:" << pbft_period << ", round:" << pbft_round;
  sealAndSend(peerID, GetNextVotesSyncPacket, std::move(dev::RLPStream(2) << pbft_period << pbft_round));
}

}  // namespace taraxa::network::tarcap
