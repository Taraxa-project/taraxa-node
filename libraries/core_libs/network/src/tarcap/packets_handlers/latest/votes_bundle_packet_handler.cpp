#include "network/tarcap/packets_handlers/latest/votes_bundle_packet_handler.hpp"

#include "pbft/pbft_manager.hpp"
#include "vote/votes_bundle_rlp.hpp"
#include "vote_manager/vote_manager.hpp"

namespace taraxa::network::tarcap {

VotesBundlePacketHandler::VotesBundlePacketHandler(const FullNodeConfig &conf, std::shared_ptr<PeersState> peers_state,
                                                   std::shared_ptr<TimePeriodPacketsStats> packets_stats,
                                                   std::shared_ptr<PbftManager> pbft_mgr,
                                                   std::shared_ptr<PbftChain> pbft_chain,
                                                   std::shared_ptr<VoteManager> vote_mgr,
                                                   std::shared_ptr<SlashingManager> slashing_manager,
                                                   const std::string &logs_prefix)
    : IVotePacketHandler(conf, std::move(peers_state), std::move(packets_stats), std::move(pbft_mgr),
                         std::move(pbft_chain), std::move(vote_mgr), std::move(slashing_manager),
                         logs_prefix + "VOTES_BUNDLE_PH") {}

void VotesBundlePacketHandler::process(const threadpool::PacketData &packet_data,
                                       const std::shared_ptr<TaraxaPeer> &peer) {
  // Decode packet rlp into packet object
  auto packet = decodePacketRlp<VotesBundlePacket>(packet_data.rlp_);

  if (packet.votes_bundle.votes.size() == 0 || packet.votes_bundle.votes.size() > kMaxVotesInBundleRlp) {
    throw InvalidRlpItemsCountException("VotesBundlePacket", packet.votes_bundle.votes.size(), kMaxVotesInBundleRlp);
  }

  const auto [current_pbft_round, current_pbft_period] = pbft_mgr_->getPbftRoundAndPeriod();

  const auto &reference_vote = packet.votes_bundle.votes.front();
  const auto votes_bundle_votes_type = reference_vote->getType();

  // Votes sync bundles are allowed to cotain only votes bundles of the same type, period, round and step so if first
  // vote is irrelevant, all of them are
  if (!isPbftRelevantVote(packet.votes_bundle.votes[0])) {
    logger_->warn(
        "Drop votes sync bundle as it is irrelevant for current pbft state. Votes (period, round, step) = ({}, {}, "
        "{}). Current PBFT (period, round, step) = ({}, {}, {})",
        reference_vote->getPeriod(), reference_vote->getRound(), current_pbft_period, reference_vote->getStep(),
        current_pbft_round, pbft_mgr_->getPbftStep());
    return;
  }

  // VotesBundlePacket does not support propose votes
  if (reference_vote->getType() == PbftVoteTypes::propose_vote) {
    logger_->error(
        "Dropping votes bundle packet due to received \"propose\" votes from {}. The peer may be a malicious player, "
        "will be disconnected",
        peer->getId());
    disconnect(peer->getId(), dev::p2p::UserReason);
    return;
  }

  // Process processStandardVote is called with false in case of next votes bundle -> does not check max boundaries
  // for round and step to actually being able to sync the current round in case network is stalled
  bool check_max_round_step = true;
  if (votes_bundle_votes_type == PbftVoteTypes::cert_vote || votes_bundle_votes_type == PbftVoteTypes::next_vote) {
    check_max_round_step = false;
  }

  size_t processed_votes_count = 0;
  for (const auto &vote : packet.votes_bundle.votes) {
    peer->markPbftVoteAsKnown(vote->getHash());

    // Do not process vote that has already been validated
    if (vote_mgr_->voteAlreadyValidated(vote->getHash())) {
      logger_->debug("Received vote {} has already been validated", vote->getHash());
      continue;
    }

    logger_->debug("Received sync vote {}", vote->getHash().abridged());

    if (!processVote(vote, nullptr, peer, check_max_round_step)) {
      continue;
    }

    processed_votes_count++;
  }

  logger_->info("Received {} (processed {} ) sync votes from peer {} node current round {}, peer pbft round {}",
                packet.votes_bundle.votes.size(), processed_votes_count, peer->getId(), current_pbft_round,
                reference_vote->getRound());

  onNewPbftVotesBundle(packet.votes_bundle.votes, false, peer->getId());
}

}  // namespace taraxa::network::tarcap
