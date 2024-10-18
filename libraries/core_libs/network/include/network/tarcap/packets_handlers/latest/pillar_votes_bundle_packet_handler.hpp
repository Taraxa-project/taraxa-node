#pragma once

#include "network/tarcap/packets/latest/pillar_votes_bundle_packet.hpp"
#include "network/tarcap/packets_handlers/latest/common/ext_pillar_vote_packet_handler.hpp"

namespace taraxa::network::tarcap {

class PillarVotesBundlePacketHandler : public ExtPillarVotePacketHandler<PillarVotesBundlePacket> {
 public:
  PillarVotesBundlePacketHandler(const FullNodeConfig& conf, std::shared_ptr<PeersState> peers_state,
                                 std::shared_ptr<TimePeriodPacketsStats> packets_stats,
                                 std::shared_ptr<pillar_chain::PillarChainManager> pillar_chain_manager,
                                 const addr_t& node_addr, const std::string& logs_prefix);

  // Packet type that is processed by this handler
  static constexpr SubprotocolPacketType kPacketType_ = SubprotocolPacketType::kPillarVotesBundlePacket;

  constexpr static size_t kMaxPillarVotesInBundleRlp{250};

 private:
  virtual void process(PillarVotesBundlePacket&& packet, const std::shared_ptr<TaraxaPeer>& peer) override;
};

}  // namespace taraxa::network::tarcap
