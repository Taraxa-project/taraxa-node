#include "network/tarcap/packets_handlers/interface/pillar_vote_packet_handler.hpp"

namespace taraxa::network::tarcap {

IPillarVotePacketHandler::IPillarVotePacketHandler(
    const FullNodeConfig& conf, std::shared_ptr<PeersState> peers_state,
    std::shared_ptr<TimePeriodPacketsStats> packets_stats,
    std::shared_ptr<pillar_chain::PillarChainManager> pillar_chain_manager, const addr_t& node_addr,
    const std::string& logs_prefix)
    : ExtPillarVotePacketHandler(conf, std::move(peers_state), std::move(packets_stats),
                                 std::move(pillar_chain_manager), node_addr, logs_prefix) {}

void IPillarVotePacketHandler::onNewPillarVote(const std::shared_ptr<PillarVote>& vote, bool rebroadcast) {
  for (const auto& peer : peers_state_->getAllPeers()) {
    if (peer.second->syncing_) {
      LOG(log_dg_) << "Pillar vote " << vote->getHash() << ", period " << vote->getPeriod() << " not sent to "
                   << peer.first << ". Peer syncing";
      continue;
    }

    if (peer.second->isPillarVoteKnown(vote->getHash()) && !rebroadcast) {
      continue;
    }

    sendPillarVote(peer.second, vote);
  }
}

}  // namespace taraxa::network::tarcap
