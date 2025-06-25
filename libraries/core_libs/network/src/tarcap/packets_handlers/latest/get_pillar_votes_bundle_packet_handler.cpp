#include "network/tarcap/packets_handlers/latest/get_pillar_votes_bundle_packet_handler.hpp"

#include "network/tarcap/packets/latest/pillar_votes_bundle_packet.hpp"
#include "network/tarcap/packets_handlers/latest/pillar_votes_bundle_packet_handler.hpp"

namespace taraxa::network::tarcap {

GetPillarVotesBundlePacketHandler::GetPillarVotesBundlePacketHandler(
    const FullNodeConfig &conf, std::shared_ptr<PeersState> peers_state,
    std::shared_ptr<TimePeriodPacketsStats> packets_stats,
    std::shared_ptr<pillar_chain::PillarChainManager> pillar_chain_manager, const std::string &logs_prefix)
    : IGetPillarVotesBundlePacketHandler(conf, std::move(peers_state), std::move(packets_stats),
                                         logs_prefix + "GET_PILLAR_VOTES_BUNDLE_PH"),
      pillar_chain_manager_(std::move(pillar_chain_manager)) {}

void GetPillarVotesBundlePacketHandler::process(const threadpool::PacketData &packet_data,
                                                const std::shared_ptr<TaraxaPeer> &peer) {
  // Decode packet rlp into packet object
  auto packet = decodePacketRlp<GetPillarVotesBundlePacket>(packet_data.rlp_);

  logger_->debug("GetPillarVotesBundlePacketHandler received from peer {}", peer->getId());

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
    logger_->debug("No pillar votes for period {}and pillar block hash {}", packet.period, packet.pillar_block_hash);
    return;
  }
  // Check if the votes size exceeds the maximum limit and split into multiple packets if needed
  const size_t total_votes = votes.size();
  size_t votes_sent = 0;

  while (votes_sent < total_votes) {
    // Determine the size of the current chunk
    const size_t chunk_size =
        std::min(PillarVotesBundlePacketHandler::kMaxPillarVotesInBundleRlp, total_votes - votes_sent);

    // Create PillarVotesBundlePacket
    std::vector<std::shared_ptr<PillarVote>> pillar_votes;
    pillar_votes.reserve(chunk_size);
    for (size_t i = 0; i < chunk_size; ++i) {
      pillar_votes.emplace_back(votes[votes_sent + i]);
    }
    PillarVotesBundlePacket pillar_votes_bundle_packet(OptimizedPillarVotesBundle{std::move(pillar_votes)});

    // Seal and send the chunk to the peer
    if (sealAndSend(peer->getId(), SubprotocolPacketType::kPillarVotesBundlePacket,
                    encodePacketRlp(pillar_votes_bundle_packet))) {
      // Mark the votes in this chunk as known
      for (size_t i = 0; i < chunk_size; ++i) {
        peer->markPillarVoteAsKnown(votes[votes_sent + i]->getHash());
      }

      logger_->debug("Pillar votes bundle for period {}, hash {} sent to {} (Chunk {}/{})", packet.period,
                     packet.pillar_block_hash, peer->getId(),
                     (votes_sent / PillarVotesBundlePacketHandler::kMaxPillarVotesInBundleRlp) + 1,
                     (total_votes + PillarVotesBundlePacketHandler::kMaxPillarVotesInBundleRlp - 1) /
                         PillarVotesBundlePacketHandler::kMaxPillarVotesInBundleRlp);
    }

    // Update the votes_sent counter
    votes_sent += chunk_size;
  }
}

void GetPillarVotesBundlePacketHandler::requestPillarVotesBundle(PbftPeriod period, const blk_hash_t &pillar_block_hash,
                                                                 const std::shared_ptr<TaraxaPeer> &peer) {
  if (sealAndSend(peer->getId(), SubprotocolPacketType::kGetPillarVotesBundlePacket,
                  encodePacketRlp(GetPillarVotesBundlePacket(period, pillar_block_hash)))) {
    logger_->debug("Requested pillar votes bundle for period {} and pillar block {} from peer {}", period,
                   pillar_block_hash, peer->getId());
  } else {
    logger_->error("Unable to send pillar votes bundle request for period {} and pillar block {} to peer {}", period,
                   pillar_block_hash, peer->getId());
  }
}

}  // namespace taraxa::network::tarcap
