#include "network/tarcap/packets_handlers/latest/votes_bundle_packet_handler.hpp"

#include "pbft/pbft_manager.hpp"
#include "vote/votes_bundle_rlp.hpp"
#include "vote_manager/vote_manager.hpp"

namespace taraxa::network::tarcap {

VotesBundlePacketHandler::VotesBundlePacketHandler(const FullNodeConfig &conf, std::shared_ptr<PeersState> peers_state,
                                                   std::shared_ptr<TimePeriodPacketsStats> packets_stats,
                                                   std::shared_ptr<PbftManager> pbft_mgr,
                                                   std::shared_ptr<PbftChain> pbft_chain,
                                                   std::shared_ptr<VoteManager> vote_mgr, const addr_t &node_addr)
    : ExtVotesPacketHandler(conf, std::move(peers_state), std::move(packets_stats), std::move(pbft_mgr),
                            std::move(pbft_chain), std::move(vote_mgr), node_addr, "VOTES_SYNC_PH") {}

void VotesBundlePacketHandler::validatePacketRlpFormat(
    [[maybe_unused]] const threadpool::PacketData &packet_data) const {
  auto items = packet_data.rlp_.itemCount();
  if (items != kVotesBundleRlpSize) {
    throw InvalidRlpItemsCountException(packet_data.type_str_, items, kVotesBundleRlpSize);
  }

  auto votes_count = packet_data.rlp_[kVotesBundleRlpSize - 1].itemCount();
  if (votes_count == 0 || votes_count > kMaxVotesInBundleRlp) {
    throw InvalidRlpItemsCountException(packet_data.type_str_, items, kMaxVotesInBundleRlp);
  }
}

void VotesBundlePacketHandler::process(const threadpool::PacketData &packet_data,
                                       const std::shared_ptr<TaraxaPeer> &peer) {
  const auto [current_pbft_round, current_pbft_period] = pbft_mgr_->getPbftRoundAndPeriod();

  const auto votes_bundle_block_hash = packet_data.rlp_[0].toHash<blk_hash_t>();
  const auto votes_bundle_pbft_period = packet_data.rlp_[1].toInt<PbftPeriod>();
  const auto votes_bundle_pbft_round = packet_data.rlp_[2].toInt<PbftRound>();
  const auto votes_bundle_votes_step = packet_data.rlp_[3].toInt<PbftStep>();

  const auto &reference_vote =
      std::make_shared<Vote>(votes_bundle_block_hash, votes_bundle_pbft_period, votes_bundle_pbft_round,
                             votes_bundle_votes_step, packet_data.rlp_[4][0]);
  const auto votes_bundle_votes_type = reference_vote->getType();

  // Votes sync bundles are allowed to cotain only votes bundles of the same type, period, round and step so if first
  // vote is irrelevant, all of them are
  if (!isPbftRelevantVote(reference_vote)) {
    LOG(log_wr_) << "Drop votes sync bundle as it is irrelevant for current pbft state. Votes (period, round, step) = ("
                 << votes_bundle_pbft_period << ", " << votes_bundle_pbft_round << ", " << reference_vote->getStep()
                 << "). Current PBFT (period, round, step) = (" << current_pbft_period << ", " << current_pbft_round
                 << ", " << pbft_mgr_->getPbftStep() << ")";
    return;
  }

  // VotesBundlePacket does not support propose votes
  if (reference_vote->getType() == PbftVoteTypes::propose_vote) {
    LOG(log_er_) << "Dropping votes bundle packet due to received \"propose\" votes from " << packet_data.from_node_id_
                 << ". The peer may be a malicious player, will be disconnected";
    disconnect(packet_data.from_node_id_, dev::p2p::UserReason);
    return;
  }

  // Process processStandardVote is called with false in case of next votes bundle -> does not check max boundaries
  // for round and step to actually being able to sync the current round in case network is stalled
  bool check_max_round_step = true;
  if (votes_bundle_votes_type == PbftVoteTypes::cert_vote || votes_bundle_votes_type == PbftVoteTypes::next_vote) {
    check_max_round_step = false;
  }

  std::vector<std::shared_ptr<Vote>> votes;
  for (const auto vote_rlp : packet_data.rlp_[4]) {
    auto vote = std::make_shared<Vote>(votes_bundle_block_hash, votes_bundle_pbft_period, votes_bundle_pbft_round,
                                       votes_bundle_votes_step, vote_rlp);
    peer->markVoteAsKnown(vote->getHash());

    // Do not process vote that has already been validated
    if (vote_mgr_->voteAlreadyValidated(vote->getHash())) {
      LOG(log_dg_) << "Received vote " << vote->getHash() << " has already been validated";
      continue;
    }

    LOG(log_dg_) << "Received sync vote " << vote->getHash().abridged();

    if (!processVote(vote, nullptr, peer, check_max_round_step)) {
      continue;
    }

    votes.push_back(std::move(vote));
  }

  LOG(log_nf_) << "Received " << packet_data.rlp_[4].itemCount() << " (processed " << votes.size()
               << " ) sync votes from peer " << packet_data.from_node_id_ << " node current round "
               << current_pbft_round << ", peer pbft round " << votes_bundle_pbft_round;

  onNewPbftVotesBundle(votes, false, packet_data.from_node_id_);
}

void VotesBundlePacketHandler::onNewPbftVotesBundle(const std::vector<std::shared_ptr<Vote>> &votes, bool rebroadcast,
                                                    const std::optional<dev::p2p::NodeID> &exclude_node) {
  for (const auto &peer : peers_state_->getAllPeers()) {
    if (peer.second->syncing_) {
      continue;
    }

    if (exclude_node.has_value() && *exclude_node == peer.first) {
      continue;
    }

    std::vector<std::shared_ptr<Vote>> peer_votes;
    for (const auto &vote : votes) {
      if (!rebroadcast && peer.second->isVoteKnown(vote->getHash())) {
        continue;
      }

      peer_votes.push_back(vote);
    }

    sendPbftVotesBundle(peer.second, std::move(peer_votes));
  }
}

}  // namespace taraxa::network::tarcap
