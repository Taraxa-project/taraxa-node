#include "network/tarcap/packets_handlers/v4/pillar_votes_bundle_packet_handler.hpp"

#include "vote/pillar_vote.hpp"

namespace taraxa::network::tarcap::v4 {

PillarVotesBundlePacketHandler::PillarVotesBundlePacketHandler(
    const FullNodeConfig &conf, std::shared_ptr<PeersState> peers_state,
    std::shared_ptr<TimePeriodPacketsStats> packets_stats,
    std::shared_ptr<pillar_chain::PillarChainManager> pillar_chain_manager, const addr_t &node_addr,
    const std::string &logs_prefix)
    : ExtPillarVotePacketHandler(conf, std::move(peers_state), std::move(packets_stats),
                                 std::move(pillar_chain_manager), node_addr, logs_prefix + "PILLAR_VOTES_BUNDLE_PH") {}

void PillarVotesBundlePacketHandler::process(PillarVotesBundlePacket &&packet,
                                             const std::shared_ptr<TaraxaPeer> &peer) {
  // TODO[2744]: there could be the same protection as in pbft syncing that only requested bundle packet is accepted
  LOG(log_dg_) << "PillarVotesBundlePacket received from peer " << peer->getId();

  for (const auto &pillar_vote : packet.pillar_votes) {
    if (!kConf.genesis.state.hardforks.ficus_hf.isFicusHardfork(pillar_vote->getPeriod())) {
      std::ostringstream err_msg;
      err_msg << "Synced pillar vote " << pillar_vote->getHash() << ", period " << pillar_vote->getPeriod()
              << " < ficus hardfork block num";
      throw MaliciousPeerException(err_msg.str());
    }

    processPillarVote(pillar_vote, peer);
  }
}

}  // namespace taraxa::network::tarcap::v4
