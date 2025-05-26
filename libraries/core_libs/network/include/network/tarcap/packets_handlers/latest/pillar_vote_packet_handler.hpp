#pragma once

#include "network/tarcap/packets/latest/pillar_vote_packet.hpp"
#include "network/tarcap/packets_handlers/interface/pillar_vote_packet_handler.hpp"

namespace taraxa::network::tarcap {

class PillarVotePacketHandler : public IPillarVotePacketHandler {
 public:
  PillarVotePacketHandler(const FullNodeConfig& conf, std::shared_ptr<PeersState> peers_state,
                          std::shared_ptr<TimePeriodPacketsStats> packets_stats,
                          std::shared_ptr<pillar_chain::PillarChainManager> pillar_chain_manager,
                          const std::string& logs_prefix = "");

  void sendPillarVote(const std::shared_ptr<TaraxaPeer>& peer, const std::shared_ptr<PillarVote>& vote) override;

  // Packet type that is processed by this handler
  static constexpr SubprotocolPacketType kPacketType_ = SubprotocolPacketType::kPillarVotePacket;

 private:
  virtual void process(const threadpool::PacketData& packet_data, const std::shared_ptr<TaraxaPeer>& peer) override;
};

}  // namespace taraxa::network::tarcap
