#include "network/tarcap/packets_handlers/votes_sync_packet_handler.hpp"

#include "pbft/pbft_manager.hpp"
#include "vote_manager/vote_manager.hpp"

namespace taraxa::network::tarcap {

VotesSyncPacketHandler::VotesSyncPacketHandler(std::shared_ptr<PeersState> peers_state,
                                               std::shared_ptr<PacketsStats> packets_stats,
                                               std::shared_ptr<PbftManager> pbft_mgr,
                                               std::shared_ptr<VoteManager> vote_mgr,
                                               std::shared_ptr<NextVotesManager> next_votes_mgr,
                                               std::shared_ptr<DbStorage> db, const addr_t &node_addr)
    : ExtVotesPacketHandler(std::move(peers_state), std::move(packets_stats), node_addr, "VOTES_SYNC_PH"),
      pbft_mgr_(std::move(pbft_mgr)),
      vote_mgr_(std::move(vote_mgr)),
      next_votes_mgr_(std::move(next_votes_mgr)),
      db_(std::move(db)) {}

void VotesSyncPacketHandler::validatePacketRlpFormat([[maybe_unused]] const PacketData &packet_data) const {
  // Number of votes is not fixed, nothing to be checked here
}

void VotesSyncPacketHandler::process(const PacketData &packet_data, const std::shared_ptr<TaraxaPeer> &peer) {
  const auto next_votes_count = packet_data.rlp_.itemCount();
  if (next_votes_count == 0) {
    LOG(log_er_) << "Receive 0 next votes from peer " << packet_data.from_node_id_
                 << ". The peer may be a malicious player, will be disconnected";
    disconnect(packet_data.from_node_id_, dev::p2p::UserReason);
    return;
  }

  auto vote = std::make_shared<Vote>(packet_data.rlp_[0].data().toBytes());

  const auto pbft_current_round = pbft_mgr_->getPbftRound();
  const auto peer_pbft_round = vote->getRound() + 1;

  std::vector<std::shared_ptr<Vote>> next_votes;
  for (size_t i = 0; i < next_votes_count; i++) {
    auto next_vote = std::make_shared<Vote>(packet_data.rlp_[i].data().toBytes());
    if (next_vote->getRound() != peer_pbft_round - 1) {
      LOG(log_er_) << "Received next votes bundle with unmatched rounds from " << packet_data.from_node_id_
                   << ". The peer may be a malicous player, will be disconnected";
      disconnect(packet_data.from_node_id_, dev::p2p::UserReason);
      return;
    }

    const auto next_vote_hash = next_vote->getHash();
    LOG(log_nf_) << "Received PBFT next vote " << next_vote_hash;
    peer->markVoteAsKnown(next_vote_hash);
    next_votes.push_back(std::move(next_vote));
  }

  LOG(log_nf_) << "Received " << next_votes_count << " next votes from peer " << packet_data.from_node_id_
               << " node current round " << pbft_current_round << ", peer pbft round " << peer_pbft_round;

  if (pbft_current_round < peer_pbft_round) {
    // Add into votes unverified queue
    for (auto &vote_n : next_votes) {
      auto vote_hash = vote_n->getHash();
      auto vote_round = vote_n->getRound();

      if (vote_mgr_->voteInUnverifiedMap(vote_round, vote_hash) || vote_mgr_->voteInVerifiedMap(vote_n)) {
        LOG(log_dg_) << "Received PBFT next vote " << vote_hash << " (from " << packet_data.from_node_id_.abridged()
                     << ") already saved in queue.";
        continue;
      }

      // Synchronization point in case multiple threads are processing the same vote at the same time
      // Adds unverified vote into local structure + database
      if (!vote_mgr_->addUnverifiedVote(vote_n)) {
        LOG(log_dg_) << "Received PBFT next vote " << vote_hash << " (from " << packet_data.from_node_id_.abridged()
                     << ") already saved in unverified queue by a different thread(race condition).";
        continue;
      }

      onNewPbftVote(std::move(vote_n));
    }
  } else if (pbft_current_round == peer_pbft_round) {
    // Update previous round next votes
    const auto pbft_2t_plus_1 = db_->getPbft2TPlus1(pbft_current_round - 1);
    if (!pbft_2t_plus_1) {
      LOG(log_er_) << "Cannot get PBFT 2t+1 in PBFT round " << pbft_current_round - 1;
      return;
    }

    // Update our previous round next vote bundles...
    next_votes_mgr_->updateWithSyncedVotes(next_votes, pbft_2t_plus_1);
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
      sendPbftNextVotes(peer_to_share_to.first, send_next_votes_bundle);
    }
  }
}

void VotesSyncPacketHandler::broadcastPreviousRoundNextVotesBundle() {
  const auto next_votes_bundle = next_votes_mgr_->getNextVotes();
  if (next_votes_bundle.empty()) {
    LOG(log_er_) << "There are empty next votes for previous PBFT round";
    return;
  }

  const auto pbft_current_round = pbft_mgr_->getPbftRound();

  for (auto const &peer : peers_state_->getAllPeers()) {
    // Nodes may vote at different values at previous round, so need less or equal
    if (!peer.second->syncing_ && peer.second->pbft_round_ <= pbft_current_round) {
      std::vector<std::shared_ptr<Vote>> send_next_votes_bundle;
      for (auto const &v : next_votes_bundle) {
        if (!peer.second->isVoteKnown(v->getHash())) {
          send_next_votes_bundle.push_back(std::move(v));
        }
      }
      sendPbftNextVotes(peer.first, send_next_votes_bundle);
    }
  }
}

}  // namespace taraxa::network::tarcap
