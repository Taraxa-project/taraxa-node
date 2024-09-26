#include "network/tarcap/packets_handlers/latest/get_pillar_votes_bundle_packet_handler.hpp"

#include "network/tarcap/packets_handlers/latest/pillar_votes_bundle_packet_handler.hpp"

namespace taraxa::network::tarcap {

GetPillarVotesBundlePacketHandler::GetPillarVotesBundlePacketHandler(
    const FullNodeConfig &conf, std::shared_ptr<PeersState> peers_state,
    std::shared_ptr<TimePeriodPacketsStats> packets_stats,
    std::shared_ptr<pillar_chain::PillarChainManager> pillar_chain_manager, const addr_t &node_addr,
    const std::string &logs_prefix)
    : PacketHandler(conf, std::move(peers_state), std::move(packets_stats), node_addr,
                    logs_prefix + "GET_PILLAR_VOTES_BUNDLE_PH"),
      pillar_chain_manager_(std::move(pillar_chain_manager)) {}

void GetPillarVotesBundlePacketHandler::process(GetPillarVotesBundlePacket &&packet,
                                                const std::shared_ptr<TaraxaPeer> &peer) {
  LOG(log_dg_) << "GetPillarVotesBundlePacketHandler received from peer " << peer->getId();

  if (!kConf.genesis.state.hardforks.ficus_hf.isFicusHardfork(packet.period)) {
    std::ostringstream err_msg;
    err_msg << "Pillar votes bundle request for period " << packet.period << ", ficus hardfork block num "
            << kConf.genesis.state.hardforks.ficus_hf.block_num;
    throw MaliciousPeerException(err_msg.str());
  }

  if (!kConf.genesis.state.hardforks.ficus_hf.isPbftWithPillarBlockPeriod(packet.period)) {
    std::ostringstream err_msg;
    err_msg << "Pillar votes bundle request for period " << packet.period << ". Wrong requested period";
    throw MaliciousPeerException(err_msg.str());
  }

  const auto votes = pillar_chain_manager_->getVerifiedPillarVotes(packet.period, packet.pillar_block_hash);
  if (votes.empty()) {
    LOG(log_dg_) << "No pillar votes for period " << packet.period << "and pillar block hash "
                 << packet.pillar_block_hash;
    return;
  }
  // Check if the votes size exceeds the maximum limit and split into multiple packets if needed
  const size_t total_votes = votes.size();
  size_t votes_sent = 0;

  while (votes_sent < total_votes) {
    // Determine the size of the current chunk
    const size_t chunk_size =
        std::min(PillarVotesBundlePacketHandler::kMaxPillarVotesInBundleRlp, total_votes - votes_sent);

    // Create a new RLPStream for the chunk
    dev::RLPStream s(chunk_size);
    for (size_t i = 0; i < chunk_size; ++i) {
      const auto &sig = votes[votes_sent + i];
      s.appendRaw(sig->rlp());
    }

    // Seal and send the chunk to the peer
    if (sealAndSend(peer->getId(), SubprotocolPacketType::kPillarVotesBundlePacket, std::move(s))) {
      // Mark the votes in this chunk as known
      for (size_t i = 0; i < chunk_size; ++i) {
        peer->markPillarVoteAsKnown(votes[votes_sent + i]->getHash());
      }

      LOG(log_nf_) << "Pillar votes bundle for period " << packet.period << ", hash " << packet.pillar_block_hash
                   << " sent to " << peer->getId() << " (Chunk "
                   << (votes_sent / PillarVotesBundlePacketHandler::kMaxPillarVotesInBundleRlp) + 1 << "/"
                   << (total_votes + PillarVotesBundlePacketHandler::kMaxPillarVotesInBundleRlp - 1) /
                          PillarVotesBundlePacketHandler::kMaxPillarVotesInBundleRlp
                   << ")";
    }

    // Update the votes_sent counter
    votes_sent += chunk_size;
  }
}

void GetPillarVotesBundlePacketHandler::requestPillarVotesBundle(PbftPeriod period, const blk_hash_t &pillar_block_hash,
                                                                 const std::shared_ptr<TaraxaPeer> &peer) {
  // TODO: create GetPillarVotesBundlePacket object and call encode instead of manullaty creating packet here
  dev::RLPStream s(kGetPillarVotesBundlePacketSize);
  s << period;
  s << pillar_block_hash;

  if (sealAndSend(peer->getId(), SubprotocolPacketType::kGetPillarVotesBundlePacket, std::move(s))) {
    LOG(log_nf_) << "Requested pillar votes bundle for period " << period << " and pillar block " << pillar_block_hash
                 << " from peer " << peer->getId();
  } else {
    LOG(log_er_) << "Unable to send pillar votes bundle request for period " << period << " and pillar block "
                 << pillar_block_hash << " to peer " << peer->getId();
  }
}

}  // namespace taraxa::network::tarcap
