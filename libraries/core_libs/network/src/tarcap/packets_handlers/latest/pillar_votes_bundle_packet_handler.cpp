#include "network/tarcap/packets_handlers/latest/pillar_votes_bundle_packet_handler.hpp"

#include "vote/pillar_vote.hpp"

namespace taraxa::network::tarcap {

PillarVotesBundlePacketHandler::PillarVotesBundlePacketHandler(
    const FullNodeConfig &conf, std::shared_ptr<PeersState> peers_state,
    std::shared_ptr<TimePeriodPacketsStats> packets_stats,
    std::shared_ptr<pillar_chain::PillarChainManager> pillar_chain_manager, const addr_t &node_addr,
    const std::string &logs_prefix)
    : ExtPillarVotePacketHandler(conf, std::move(peers_state), std::move(packets_stats),
                                 std::move(pillar_chain_manager), node_addr, logs_prefix + "PILLAR_VOTES_BUNDLE_PH") {}

void PillarVotesBundlePacketHandler::validatePacketRlpFormat(
    [[maybe_unused]] const threadpool::PacketData &packet_data) const {
  auto items = packet_data.rlp_.itemCount();
  if (items == 0 || items > kMaxPillarVotesInBundleRlp) {
    throw InvalidRlpItemsCountException(packet_data.type_str_, items, kMaxPillarVotesInBundleRlp);
  }
}

void PillarVotesBundlePacketHandler::process(const threadpool::PacketData &packet_data,
                                             const std::shared_ptr<TaraxaPeer> &peer) {
  // TODO: there could be the same protection as in pbft syncing that only requested bundle packet is accepted
  LOG(log_dg_) << "PillarVotesBundlePacket received from peer " << peer->getId();

  for (const auto vote_rlp : packet_data.rlp_) {
    const auto pillar_vote = std::make_shared<PillarVote>(vote_rlp);
    if (!kConf.genesis.state.hardforks.ficus_hf.isFicusHardfork(pillar_vote->getPeriod())) {
      std::ostringstream err_msg;
      err_msg << "Synced pillar vote " << pillar_vote->getHash() << ", period " << pillar_vote->getPeriod()
              << " < ficus hardfork block num";
      throw MaliciousPeerException(err_msg.str());
    }

    processPillarVote(pillar_vote, peer);
  }
}

}  // namespace taraxa::network::tarcap
