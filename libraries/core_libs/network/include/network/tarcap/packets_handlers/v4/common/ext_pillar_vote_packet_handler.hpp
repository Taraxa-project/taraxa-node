#pragma once

#include "packet_handler.hpp"

namespace taraxa {
class KeyManager;
class PillarVote;

namespace pillar_chain {
class PillarChainManager;
}  // namespace pillar_chain

namespace final_chain {
class FinalChain;
}

}  // namespace taraxa

namespace taraxa::network::tarcap::v4 {

class ExtPillarVotePacketHandler : public PacketHandler {
 public:
  ExtPillarVotePacketHandler(const FullNodeConfig& conf, std::shared_ptr<PeersState> peers_state,
                             std::shared_ptr<TimePeriodPacketsStats> packets_stats,
                             std::shared_ptr<pillar_chain::PillarChainManager> pillar_chain_manager,
                             const addr_t& node_addr, const std::string& log_channel);

 protected:
  bool processPillarVote(const std::shared_ptr<PillarVote>& vote, const std::shared_ptr<TaraxaPeer>& peer);

 protected:
  std::shared_ptr<pillar_chain::PillarChainManager> pillar_chain_manager_;
};

}  // namespace taraxa::network::tarcap::v4
