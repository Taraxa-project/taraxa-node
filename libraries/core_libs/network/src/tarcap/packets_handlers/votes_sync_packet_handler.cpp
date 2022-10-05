#include "network/tarcap/packets_handlers/votes_sync_packet_handler.hpp"

#include "pbft/pbft_manager.hpp"
#include "vote_manager/vote_manager.hpp"

namespace taraxa::network::tarcap {

VotesSyncPacketHandler::VotesSyncPacketHandler(
    std::shared_ptr<PeersState> peers_state, std::shared_ptr<PacketsStats> packets_stats,
    std::shared_ptr<PbftManager> pbft_mgr, std::shared_ptr<PbftChain> pbft_chain, std::shared_ptr<VoteManager> vote_mgr,
    std::shared_ptr<NextVotesManager> next_votes_mgr, std::shared_ptr<DbStorage> db, const NetworkConfig &net_config,
    const addr_t &node_addr)
    : ExtVotesPacketHandler(std::move(peers_state), std::move(packets_stats), std::move(pbft_mgr),
                            std::move(pbft_chain), std::move(vote_mgr), net_config, node_addr, "VOTES_SYNC_PH"),
      next_votes_mgr_(std::move(next_votes_mgr)),
      db_(std::move(db)) {}

void VotesSyncPacketHandler::validatePacketRlpFormat([[maybe_unused]] const PacketData &packet_data) const {
  auto items = packet_data.rlp_.itemCount();
  if (items == 0 || items > kMaxVotesInPacket) {
    throw InvalidRlpItemsCountException(packet_data.type_str_, items, kMaxVotesInPacket);
  }
}

void VotesSyncPacketHandler::process(const PacketData &packet_data, const std::shared_ptr<TaraxaPeer> &peer) {
  // We already have 2t+1 votes for both NULL_BLOCK_HASH as well as some specific block hash
  if (next_votes_mgr_->enoughNextVotes()) {
    LOG(log_nf_) << "Already have enought next votes for perevious round.";
    return;
  }

  auto reference_vote = std::make_shared<Vote>(packet_data.rlp_[0].data().toBytes());

  const auto [pbft_current_round, pbft_current_period] = pbft_mgr_->getPbftRoundAndPeriod();
  const auto peer_pbft_period = reference_vote->getPeriod();
  const auto peer_pbft_round = reference_vote->getRound();

  // Accept only votes, which period is >= current pbft period
  if (peer_pbft_period < pbft_current_period) {
    LOG(log_er_) << "Dropping votes sync packet due to period. Votes period: " << peer_pbft_period
                 << ", current pbft period: " << peer_pbft_period;
    return;
  }

  // Accept only votes, which round is >= previous round(current pbft round - 1) in case their period == current pbft
  // period
  if (peer_pbft_period == pbft_current_period && peer_pbft_round < pbft_current_round - 1) {
    LOG(log_er_) << "Dropping votes sync packet due to round. Votes round: " << peer_pbft_round
                 << ", current pbft round: " << pbft_current_round;
    return;
  }

  std::vector<std::shared_ptr<Vote>> next_votes;
  blk_hash_t voted_value = NULL_BLOCK_HASH;

  const auto next_votes_count = packet_data.rlp_.itemCount();
  //  It is done in separate cycle because we don't need to process this next_votes if some of checks will fail
  for (size_t i = 0; i < next_votes_count; i++) {
    auto vote = std::make_shared<Vote>(packet_data.rlp_[i].data().toBytes());
    if (voted_value == NULL_BLOCK_HASH && vote->getBlockHash() != NULL_BLOCK_HASH) {
      // initialize voted value with first block hash that not equal to NULL_BLOCK_HASH
      voted_value = vote->getBlockHash();
    } else if (vote->getBlockHash() != NULL_BLOCK_HASH && voted_value != NULL_BLOCK_HASH &&
               vote->getBlockHash() != voted_value) {
      // we see different voted value, so bundle is invalid
      LOG(log_er_) << "Received next votes bundle with unmatched voted values(" << voted_value << ", "
                   << vote->getBlockHash() << ") from " << packet_data.from_node_id_
                   << ". The peer may be a malicious player, will be disconnected";
      disconnect(packet_data.from_node_id_, dev::p2p::UserReason);
      return;
    }

    // VotesSyncPacket is only for next votes
    if (vote->getType() != PbftVoteTypes::next_vote_type || vote->getStep() < PbftStates::finish_state) {
      LOG(log_er_) << "Received next votes bundle with non \"next_votes\" from " << packet_data.from_node_id_
                   << ". The peer may be a malicious player, will be disconnected";
      disconnect(packet_data.from_node_id_, dev::p2p::UserReason);
      return;
    }

    if (vote->getRound() != peer_pbft_round) {
      LOG(log_er_) << "Received next votes bundle with unmatched rounds from " << packet_data.from_node_id_
                   << ". The peer may be a malicious player, will be disconnected";
      disconnect(packet_data.from_node_id_, dev::p2p::UserReason);
      return;
    }

    if (vote->getPeriod() != peer_pbft_period) {
      LOG(log_er_) << "Received next votes bundle with unmatched periods from " << packet_data.from_node_id_
                   << ". The peer may be a malicious player, will be disconnected";
      disconnect(packet_data.from_node_id_, dev::p2p::UserReason);
      return;
    }

    LOG(log_dg_) << "Received sync vote " << vote->getHash().abridged();

    // Previous round vote
    if (peer_pbft_period == pbft_current_period && (pbft_current_round - 1) == peer_pbft_round) {
      if (!processNextSyncVote(vote, nullptr)) {
        continue;
      }
    } else {
      // Standard vote -> peer_pbft_period > pbft_current_period || pbft_current_round >= peer_pbft_round
      // Process processStandardVote is called with false -> does not check max boundaries for round and step to
      // actually being able to sync the current round in case network is stalled
      if (!processStandardVote(vote, nullptr, peer, false)) {
        continue;
      }
    }

    peer->markVoteAsKnown(vote->getHash());
    next_votes.push_back(std::move(vote));
  }

  LOG(log_nf_) << "Received " << next_votes_count << " (processed " << next_votes.size() << " ) sync votes from peer "
               << packet_data.from_node_id_ << " node current round " << pbft_current_round << ", peer pbft round "
               << peer_pbft_round;

  // Previous round votes
  if (peer_pbft_period == pbft_current_period && (pbft_current_round - 1) == peer_pbft_round) {
    // CONCERN... quite unsure about the following modification...

    // Update our previous round next vote bundles...
    next_votes_mgr_->updateWithSyncedVotes(next_votes, pbft_mgr_->getTwoTPlusOne());
    // Pass them on to our peers...
    const auto updated_next_votes_size = next_votes_mgr_->getNextVotesWeight();
    for (auto const &peer_to_share_to : peers_state_->getAllPeers()) {
      // Do not send votes right back to same peer...
      if (peer_to_share_to.first == packet_data.from_node_id_) {
        continue;
      }

      // Do not send votes to nodes that already have as many bundles as we do...
      if (peer_to_share_to.second->pbft_previous_round_next_votes_size_ >= updated_next_votes_size) {
        continue;
      }

      // Nodes may vote at different values at previous round, so need less or equal
      if (!peer_to_share_to.second->syncing_ && peer_to_share_to.second->pbft_round_ > pbft_current_round) {
        continue;
      }

      std::vector<std::shared_ptr<Vote>> send_next_votes_bundle;
      for (const auto &v : next_votes) {
        if (!peer_to_share_to.second->isVoteKnown(v->getHash())) {
          send_next_votes_bundle.push_back(v);
        }
      }
      sendPbftVotes(peer_to_share_to.second, std::move(send_next_votes_bundle), true);
    }
  } else {
    // Standard votes -> peer_pbft_period > pbft_current_period || (peer_pbft_period == pbft_current_period &&
    // peer_pbft_round > pbft_current_round - 1)
    onNewPbftVotes(std::move(next_votes));
  }
}

void VotesSyncPacketHandler::broadcastPreviousRoundNextVotesBundle(bool rebroadcast) {
  auto next_votes_bundle = next_votes_mgr_->getNextVotes();
  if (next_votes_bundle.empty()) {
    LOG(log_er_) << "There are empty next votes for previous PBFT round";
    return;
  }

  const auto pbft_current_round = pbft_mgr_->getPbftRound();

  for (auto const &peer : peers_state_->getAllPeers()) {
    // Nodes may vote at different values at previous round, so need less or equal
    if (!peer.second->syncing_ && peer.second->pbft_round_ <= pbft_current_round) {
      std::vector<std::shared_ptr<Vote>> send_next_votes_bundle;
      for (const auto &v : next_votes_bundle) {
        if (rebroadcast || !peer.second->isVoteKnown(v->getHash())) {
          send_next_votes_bundle.push_back(v);
        }
      }
      sendPbftVotes(peer.second, std::move(send_next_votes_bundle), true);
    }
  }
}

}  // namespace taraxa::network::tarcap
