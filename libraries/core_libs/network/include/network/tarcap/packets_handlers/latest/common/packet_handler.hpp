#pragma once

#include <libdevcore/RLP.h>

#include <memory>
#include <string_view>

#include "exceptions.hpp"
#include "logger/logger.hpp"
#include "network/tarcap/packet_types.hpp"
#include "network/tarcap/packets/latest/get_next_votes_bundle_packet.hpp"
#include "network/tarcap/packets_handlers/latest/common/base_packet_handler.hpp"
#include "network/tarcap/packets_handlers/latest/common/exceptions.hpp"
#include "network/tarcap/shared_states/peers_state.hpp"
#include "network/tarcap/stats/time_period_packets_stats.hpp"
#include "network/tarcap/taraxa_peer.hpp"
#include "network/threadpool/packet_data.hpp"

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
template <class PacketType>
class PacketHandler : public BasePacketHandler {
 public:
  PacketHandler(const FullNodeConfig& conf, std::shared_ptr<PeersState> peers_state,
                std::shared_ptr<TimePeriodPacketsStats> packets_stats, const addr_t& node_addr,
                const std::string& log_channel_name)
      : kConf(conf), peers_state_(std::move(peers_state)), packets_stats_(std::move(packets_stats)) {
    LOG_OBJECTS_CREATE(log_channel_name);
  }

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
  void processPacket(const threadpool::PacketData& packet_data) override {
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

      // Decode packet rlp into packet object
      auto packet = decodePacketRlp<PacketType>(packet_data.rlp_);

      // Main processing function
      process(std::move(packet), peer.first);

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

  // TODO: probbaly should not be here but in specific packet class ???
  void requestPbftNextVotesAtPeriodRound(const dev::p2p::NodeID& peerID, PbftPeriod pbft_period, PbftRound pbft_round) {
    LOG(log_dg_) << "Sending GetNextVotesSyncPacket with period:" << pbft_period << ", round:" << pbft_round;
    const auto packet = GetNextVotesBundlePacket{.peer_pbft_period = pbft_period, .peer_pbft_round = pbft_round};
    sealAndSend(peerID, SubprotocolPacketType::kGetNextVotesSyncPacket, encodePacketRlp(packet));
  }

 private:
  void handle_caught_exception(std::string_view exception_msg, const threadpool::PacketData& packet_data,
                               const dev::p2p::NodeID& peer,
                               dev::p2p::DisconnectReason disconnect_reason = dev::p2p::DisconnectReason::UserReason,
                               bool set_peer_as_malicious = false) {
    LOG(log_er_) << "Exception caught during packet processing: " << exception_msg << " ."
                 << "PacketData: " << jsonToUnstyledString(packet_data.getPacketDataJson())
                 << ", disconnect peer: " << peer.toString();

    if (set_peer_as_malicious) {
      peers_state_->set_peer_malicious(peer);
    }

    disconnect(peer, disconnect_reason);
  }

  /**
   * @brief Main packet processing function
   */
  virtual void process(PacketType&& packet, const std::shared_ptr<TaraxaPeer>& peer) = 0;

 protected:
  bool sealAndSend(const dev::p2p::NodeID& node_id, SubprotocolPacketType packet_type, dev::bytes&& rlp_bytes) {
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
    const size_t packet_size = rlp_bytes.size();

    host->send(node_id, TARAXA_CAPABILITY_NAME, packet_type, std::move(rlp_bytes),
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

  void disconnect(const dev::p2p::NodeID& node_id, dev::p2p::DisconnectReason reason) {
    if (auto host = peers_state_->host_.lock(); host) {
      LOG(log_nf_) << "Disconnect node " << node_id.abridged();
      host->disconnect(node_id, reason);
    } else {
      LOG(log_er_) << "Unable to disconnect node " << node_id.abridged() << " due to invalid host.";
    }
  }

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
