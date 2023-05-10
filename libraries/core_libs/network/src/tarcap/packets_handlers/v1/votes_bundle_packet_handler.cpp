#include "network/tarcap/packets_handlers/v1/votes_bundle_packet_handler.hpp"

#include "pbft/pbft_manager.hpp"
#include "vote_manager/vote_manager.hpp"

namespace taraxa::network::tarcap::v1 {

void VotesBundlePacketHandler::validatePacketRlpFormat(
    [[maybe_unused]] const threadpool::PacketData &packet_data) const {
  auto items = packet_data.rlp_.itemCount();
  if (items == 0 || items > kMaxVotesInBundleRlp) {
    throw InvalidRlpItemsCountException(packet_data.type_str_, items, kMaxVotesInBundleRlp);
  }
}

void VotesBundlePacketHandler::process(const threadpool::PacketData &packet_data,
                                       const std::shared_ptr<tarcap::TaraxaPeer> &peer) {
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

  // VotesBundlePacket does not support propose votes
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
    bool check_max_round_step = true;
    if (votes_bundle_votes_type == PbftVoteTypes::cert_vote || votes_bundle_votes_type == PbftVoteTypes::next_vote) {
      check_max_round_step = false;
    }

    if (!processVote(vote, nullptr, peer, check_max_round_step)) {
      continue;
    }

    votes.push_back(std::move(vote));
  }

  LOG(log_nf_) << "Received " << next_votes_count << " (processed " << votes.size() << " ) sync votes from peer "
               << packet_data.from_node_id_ << " node current round " << current_pbft_round << ", peer pbft round "
               << votes_bundle_pbft_round;

  onNewPbftVotesBundle(votes, false, packet_data.from_node_id_);
}

void VotesBundlePacketHandler::sendPbftVotesBundle(const std::shared_ptr<tarcap::TaraxaPeer> &peer,
                                                   std::vector<std::shared_ptr<Vote>> &&votes) {
  if (votes.empty()) {
    return;
  }

  size_t index = 0;
  while (index < votes.size()) {
    const size_t count = std::min(static_cast<size_t>(kMaxVotesInBundleRlp), votes.size() - index);
    dev::RLPStream s(count);
    for (auto i = index; i < index + count; i++) {
      const auto &vote = votes[i];
      s.appendRaw(vote->rlp(true, false));
      LOG(log_dg_) << "Send vote " << vote->getHash() << " to peer " << peer->getId();
    }

    if (sealAndSend(peer->getId(), SubprotocolPacketType::VotesBundlePacket, std::move(s))) {
      LOG(log_dg_) << count << " PBFT votes to were sent to " << peer->getId();
      for (auto i = index; i < index + count; i++) {
        peer->markVoteAsKnown(votes[i]->getHash());
      }
    }

    index += count;
  }
}

}  // namespace taraxa::network::tarcap::v1
