#pragma once

#include "network/tarcap/packets_handlers/latest/common/ext_pillar_vote_packet_handler.hpp"

namespace taraxa::network::tarcap {

class IPillarVotePacketHandler : public ExtPillarVotePacketHandler {
 public:
  IPillarVotePacketHandler(const FullNodeConfig& conf, std::shared_ptr<PeersState> peers_state,
                           std::shared_ptr<TimePeriodPacketsStats> packets_stats,
                           std::shared_ptr<pillar_chain::PillarChainManager> pillar_chain_manager,
                           const std::string& logs_prefix);

  void onNewPillarVote(const std::shared_ptr<PillarVote>& vote, bool rebroadcast = false);
  virtual void sendPillarVote(const std::shared_ptr<TaraxaPeer>& peer, const std::shared_ptr<PillarVote>& vote) = 0;
};

}  // namespace taraxa::network::tarcap
