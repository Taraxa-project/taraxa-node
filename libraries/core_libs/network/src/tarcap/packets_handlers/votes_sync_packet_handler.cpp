#include "network/tarcap/packets_handlers/votes_sync_packet_handler.hpp"

#include "pbft/pbft_manager.hpp"
#include "vote_manager/vote_manager.hpp"

namespace taraxa::network::tarcap {

VotesSyncPacketHandler::VotesSyncPacketHandler(const FullNodeConfig &conf, std::shared_ptr<PeersState> peers_state,
                                               std::shared_ptr<TimePeriodPacketsStats> packets_stats,
                                               std::shared_ptr<PbftManager> pbft_mgr,
                                               std::shared_ptr<PbftChain> pbft_chain,
                                               std::shared_ptr<VoteManager> vote_mgr, const addr_t &node_addr)
    : ExtVotesPacketHandler(conf, std::move(peers_state), std::move(packets_stats), std::move(pbft_mgr),
                            std::move(pbft_chain), std::move(vote_mgr), node_addr, "VOTES_SYNC_PH") {}

void VotesSyncPacketHandler::validatePacketRlpFormat([[maybe_unused]] const PacketData &packet_data) const {
  auto items = packet_data.rlp_.itemCount();
  if (items == 0 || items > kMaxVotesInPacket) {
    throw InvalidRlpItemsCountException(packet_data.type_str_, items, kMaxVotesInPacket);
  }
}

void VotesSyncPacketHandler::process(const PacketData &packet_data, const std::shared_ptr<TaraxaPeer> &peer) {
  const auto reference_vote = std::make_shared<Vote>(packet_data.rlp_[0]);

  const auto [current_pbft_round, current_pbft_period] = pbft_mgr_->getPbftRoundAndPeriod();
  const auto votes_bundle_pbft_period = reference_vote->getPeriod();
  const auto votes_bundle_pbft_round = reference_vote->getRound();
  const auto votes_bundle_votes_type = reference_vote->getType();
  const auto votes_bundle_voted_block = reference_vote->getBlockHash();

  // Votes sync bundles are allowed to cotain only votes bundles of the same type, period, round and step so if first
  // vote is irrelevant, all of them are
  if (!isPbftRelevantVote(reference_vote)) {
    LOG(log_wr_) << "Drop votes sync bundle as it is irrelevant for current pbft state. Votes (period, round, step) = ("
                 << votes_bundle_pbft_period << ", " << votes_bundle_pbft_round << ", " << reference_vote->getStep()
                 << "). Current PBFT (period, round, step) = (" << current_pbft_period << ", " << current_pbft_round
                 << ", " << pbft_mgr_->getPbftStep() << ")";
    return;
  }

  // VotesSyncPacket does not support propose votes
  if (votes_bundle_votes_type == PbftVoteTypes::propose_vote) {
    LOG(log_er_) << "Dropping votes sync packet due to received \"propose_votes\" votes from "
                 << packet_data.from_node_id_ << ". The peer may be a malicious player, will be disconnected";
    disconnect(packet_data.from_node_id_, dev::p2p::UserReason);
    return;
  }

  std::vector<std::shared_ptr<Vote>> votes;
  blk_hash_t next_votes_bundle_voted_block = kNullBlockHash;

  const auto next_votes_count = packet_data.rlp_.itemCount();
  for (size_t i = 0; i < next_votes_count; i++) {
    auto vote = std::make_shared<Vote>(packet_data.rlp_[i]);
    peer->markVoteAsKnown(vote->getHash());

    // Do not process vote that has already been validated
    if (vote_mgr_->voteAlreadyValidated(vote->getHash())) {
      LOG(log_dg_) << "Received vote " << vote->getHash() << " has already been validated";
      continue;
    }

    // Next votes bundle can contain votes for kNullBlockHash as well as some specific block hash
    if (vote->getType() == PbftVoteTypes::next_vote) {
      if (next_votes_bundle_voted_block == kNullBlockHash && vote->getBlockHash() != kNullBlockHash) {
        // initialize voted value with first block hash not equal to kNullBlockHash
        next_votes_bundle_voted_block = vote->getBlockHash();
      }

      if (vote->getBlockHash() != kNullBlockHash && vote->getBlockHash() != next_votes_bundle_voted_block) {
        // we see different voted value, so bundle is invalid
        LOG(log_er_) << "Received next votes bundle with unmatched voted values(" << next_votes_bundle_voted_block
                     << ", " << vote->getBlockHash() << ") from " << packet_data.from_node_id_
                     << ". The peer may be a malicious player, will be disconnected";
        disconnect(packet_data.from_node_id_, dev::p2p::UserReason);
        return;
      }
    } else {
      // Other votes bundles can contain votes only for 1 specific block hash
      if (vote->getBlockHash() != votes_bundle_voted_block) {
        // we see different voted value, so bundle is invalid
        LOG(log_er_) << "Received votes bundle with unmatched voted values(" << votes_bundle_voted_block << ", "
                     << vote->getBlockHash() << ") from " << packet_data.from_node_id_
                     << ". The peer may be a malicious player, will be disconnected";
        disconnect(packet_data.from_node_id_, dev::p2p::UserReason);
        return;
      }
    }

    if (vote->getType() != votes_bundle_votes_type) {
      LOG(log_er_) << "Received votes bundle with unmatched types from " << packet_data.from_node_id_
                   << ". The peer may be a malicious player, will be disconnected";
      disconnect(packet_data.from_node_id_, dev::p2p::UserReason);
      return;
    }

    if (vote->getPeriod() != votes_bundle_pbft_period) {
      LOG(log_er_) << "Received votes bundle with unmatched periods from " << packet_data.from_node_id_
                   << ". The peer may be a malicious player, will be disconnected";
      disconnect(packet_data.from_node_id_, dev::p2p::UserReason);
      return;
    }

    if (vote->getRound() != votes_bundle_pbft_round) {
      LOG(log_er_) << "Received votes bundle with unmatched rounds from " << packet_data.from_node_id_
                   << ". The peer may be a malicious player, will be disconnected";
      disconnect(packet_data.from_node_id_, dev::p2p::UserReason);
      return;
    }

    LOG(log_dg_) << "Received sync vote " << vote->getHash().abridged();

    // Process processStandardVote is called with false in case of next votes bundle -> does not check max boundaries
    // for round and step to actually being able to sync the current round in case network is stalled
    bool check_max_round_step = votes_bundle_votes_type == PbftVoteTypes::next_vote ? false : true;
    if (votes_bundle_votes_type == PbftVoteTypes::cert_vote) check_max_round_step = false;
    processVote(vote, nullptr, peer, check_max_round_step);
    votes.push_back(std::move(vote));
  }

  LOG(log_nf_) << "Received " << next_votes_count << " (processed " << votes.size() << " ) sync votes from peer "
               << packet_data.from_node_id_ << " node current round " << current_pbft_round << ", peer pbft round "
               << votes_bundle_pbft_round;

  onNewPbftVotesBundle(std::move(votes), false, packet_data.from_node_id_);
}

void VotesSyncPacketHandler::onNewPbftVotesBundle(std::vector<std::shared_ptr<Vote>> &&votes, bool rebroadcast,
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
