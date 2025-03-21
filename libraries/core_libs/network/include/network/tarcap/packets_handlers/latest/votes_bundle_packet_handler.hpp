#pragma once

#include "common/ext_votes_packet_handler.hpp"
#include "network/tarcap/packets/latest/votes_bundle_packet.hpp"
#include "network/tarcap/packets_handlers/interface/vote_packet_handler.hpp"

namespace taraxa::network::tarcap {

class VotesBundlePacketHandler : public IVotePacketHandler {
 public:
  VotesBundlePacketHandler(const FullNodeConfig& conf, std::shared_ptr<PeersState> peers_state,
                           std::shared_ptr<TimePeriodPacketsStats> packets_stats, std::shared_ptr<PbftManager> pbft_mgr,
                           std::shared_ptr<PbftChain> pbft_chain, std::shared_ptr<VoteManager> vote_mgr,
                           std::shared_ptr<SlashingManager> slashing_manager, const addr_t& node_addr,
                           const std::string& logs_prefix = "");

  // Packet type that is processed by this handler
  static constexpr SubprotocolPacketType kPacketType_ = SubprotocolPacketType::kVotesBundlePacket;

 private:
  virtual void process(const threadpool::PacketData& packet_data, const std::shared_ptr<TaraxaPeer>& peer) override;
};

}  // namespace taraxa::network::tarcap
