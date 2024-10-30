#include "network/tarcap/packets_handlers/latest/pillar_votes_bundle_packet_handler.hpp"

#include "network/tarcap/packets_handlers/latest/get_pillar_votes_bundle_packet_handler.hpp"
#include "vote/pillar_vote.hpp"

namespace taraxa::network::tarcap {

PillarVotesBundlePacketHandler::PillarVotesBundlePacketHandler(
    const FullNodeConfig &conf, std::shared_ptr<PeersState> peers_state,
    std::shared_ptr<TimePeriodPacketsStats> packets_stats,
    std::shared_ptr<pillar_chain::PillarChainManager> pillar_chain_manager, const addr_t &node_addr,
    const std::string &logs_prefix)
    : ExtPillarVotePacketHandler(conf, std::move(peers_state), std::move(packets_stats),
                                 std::move(pillar_chain_manager), node_addr, logs_prefix + "PILLAR_VOTES_BUNDLE_PH"),
      votes_requested_node_id_{} {}

void PillarVotesBundlePacketHandler::validatePacketRlpFormat(const threadpool::PacketData &packet_data) const {
  auto items = packet_data.rlp_.itemCount();
  if (items == 0 || items > kMaxPillarVotesInBundleRlp) {
    throw InvalidRlpItemsCountException(packet_data.type_str_, items, kMaxPillarVotesInBundleRlp);
  }
}

void PillarVotesBundlePacketHandler::process(const threadpool::PacketData &packet_data,
                                             const std::shared_ptr<TaraxaPeer> &peer) {
  LOG(log_dg_) << "PillarVotesBundlePacket received from peer " << peer->getId();

  if (const auto requested_node_id = votes_requested_node_id_.load(); requested_node_id != packet_data.from_node_id_) {
    LOG(log_wr_) << "PillarVotesBundlePacket received from unexpected peer " << packet_data.from_node_id_.abridged()
                 << " current pillar syncing peer " << requested_node_id;
    return;
  }

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

void PillarVotesBundlePacketHandler::requestPillarVotesBundle(PbftPeriod period, const blk_hash_t &pillar_block_hash,
                                                              const std::shared_ptr<TaraxaPeer> &peer) {
  dev::RLPStream s(GetPillarVotesBundlePacketHandler::kGetPillarVotesBundlePacketSize);
  s << period;
  s << pillar_block_hash;

  if (sealAndSend(peer->getId(), SubprotocolPacketType::GetPillarVotesBundlePacket, std::move(s))) {
    LOG(log_nf_) << "Requested pillar votes bundle for period " << period << " and pillar block " << pillar_block_hash
                 << " from peer " << peer->getId();
    votes_requested_node_id_ = peer->getId();
  } else {
    LOG(log_er_) << "Unable to send pillar votes bundle request for period " << period << " and pillar block "
                 << pillar_block_hash << " to peer " << peer->getId();
  }
}

}  // namespace taraxa::network::tarcap
