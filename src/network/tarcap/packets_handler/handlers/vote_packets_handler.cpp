#include "vote_packets_handler.hpp"

#include "consensus/pbft_manager.hpp"
#include "consensus/vote.hpp"

namespace taraxa::network::tarcap {

VotePacketsHandler::VotePacketsHandler(std::shared_ptr<PeersState> peers_state, std::shared_ptr<PbftManager> pbft_mgr,
                                       std::shared_ptr<VoteManager> vote_mgr,
                                       std::shared_ptr<NextVotesForPreviousRound> next_votes_mgr,
                                       std::shared_ptr<DbStorage> db, const addr_t &node_addr)
    : PacketHandler(std::move(peers_state), node_addr, "VOTE_PH"),
      pbft_mgr_(pbft_mgr),
      vote_mgr_(vote_mgr),
      next_votes_mgr_(next_votes_mgr),
      db_(db) {}

void VotePacketsHandler::process(const PacketData &packet_data, const dev::RLP &packet_rlp) {
  if (packet_data.type_ == PbftVotePacket) {
    processPbftVotePacket(packet_data, packet_rlp);
  } else if (packet_data.type_ == GetPbftNextVotes) {
    processGetPbftNextVotePacket(packet_data, packet_rlp);
  } else if (packet_data.type_ == PbftNextVotesPacket) {
    processPbftNextVotesPacket(packet_data, packet_rlp);
  } else {
    assert(false);
  }
}

inline void VotePacketsHandler::processPbftVotePacket(const PacketData & /*packet_data*/, const dev::RLP &packet_rlp) {
  LOG(log_dg_) << "In PbftVotePacket";

  Vote vote(packet_rlp[0].toBytes(), false);
  const auto vote_hash = vote.getHash();
  LOG(log_dg_) << "Received PBFT vote " << vote_hash;

  const auto pbft_round = pbft_mgr_->getPbftRound();
  const auto vote_round = vote.getRound();

  if (vote_round >= pbft_round) {
    if (!vote_mgr_->voteInUnverifiedMap(vote_round, vote_hash) &&
        !vote_mgr_->voteInVerifiedMap(vote_round, vote_hash)) {
      db_->saveUnverifiedVote(vote);
      vote_mgr_->addUnverifiedVote(vote);

      peer_->markVoteAsKnown(vote_hash);

      onNewPbftVote(vote);
    }
  }
}

inline void VotePacketsHandler::processGetPbftNextVotePacket(const PacketData &packet_data,
                                                             const dev::RLP &packet_rlp) {
  LOG(log_dg_) << "Received GetPbftNextVotes request";

  const uint64_t peer_pbft_round = packet_rlp[0].toPositiveInt64();
  const size_t peer_pbft_previous_round_next_votes_size = packet_rlp[1].toInt<unsigned>();
  const uint64_t pbft_round = pbft_mgr_->getPbftRound();
  const size_t pbft_previous_round_next_votes_size = next_votes_mgr_->getNextVotesSize();

  if (pbft_round > peer_pbft_round || (pbft_round == peer_pbft_round && pbft_previous_round_next_votes_size >
                                                                            peer_pbft_previous_round_next_votes_size)) {
    LOG(log_dg_) << "Current PBFT round is " << pbft_round << " previous round next votes size "
                 << pbft_previous_round_next_votes_size << ", and peer PBFT round is " << peer_pbft_round
                 << " previous round next votes size " << peer_pbft_previous_round_next_votes_size
                 << ". Will send out bundle of next votes";

    auto next_votes_bundle = next_votes_mgr_->getNextVotes();
    std::vector<Vote> send_next_votes_bundle;
    for (auto const &v : next_votes_bundle) {
      if (!peer_->isVoteKnown(v.getHash())) {
        send_next_votes_bundle.emplace_back(v);
      }
    }
    sendPbftNextVotes(packet_data.from_node_id_, send_next_votes_bundle);
  }
}

inline void VotePacketsHandler::processPbftNextVotesPacket(const PacketData &packet_data, const dev::RLP &packet_rlp) {
  const auto next_votes_count = packet_rlp.itemCount();
  if (next_votes_count == 0) {
    LOG(log_er_) << "Receive 0 next votes from peer " << packet_data.from_node_id_
                 << ". The peer may be a malicous player, will be disconnected";
    host_->disconnect(packet_data.from_node_id_, p2p::UserReason);

    return;
  }

  const auto pbft_currentpacket_rlpound = pbft_mgr_->getPbftRound();
  Vote vote(packet_rlp[0].data().toBytes());
  const auto peer_pbftpacket_rlpound = vote.getRound() + 1;
  std::vector<Vote> next_votes;
  for (size_t i = 0; i < next_votes_count; i++) {
    Vote next_vote(packet_rlp[i].data().toBytes());
    if (next_vote.getRound() != peer_pbftpacket_rlpound - 1) {
      LOG(log_er_) << "Received next votes bundle with unmatched rounds from " << packet_data.from_node_id_
                   << ". The peer may be a malicous player, will be disconnected";
      host_->disconnect(packet_data.from_node_id_, p2p::UserReason);

      return;
    }
    const auto next_vote_hash = next_vote.getHash();
    LOG(log_nf_) << "Received PBFT next vote " << next_vote_hash;
    peer_->markVoteAsKnown(next_vote_hash);
    next_votes.emplace_back(next_vote);
  }
  LOG(log_nf_) << "Received " << next_votes_count << " next votes from peer " << packet_data.from_node_id_
               << " node current round " << pbft_currentpacket_rlpound << ", peer pbft round "
               << peer_pbftpacket_rlpound;

  if (pbft_currentpacket_rlpound < peer_pbftpacket_rlpound) {
    // Add into votes unverified queue
    for (auto const &vote : next_votes) {
      auto vote_hash = vote.getHash();
      auto votepacket_rlpound = vote.getRound();
      if (!vote_mgr_->voteInUnverifiedMap(votepacket_rlpound, vote_hash) &&
          !vote_mgr_->voteInVerifiedMap(votepacket_rlpound, vote_hash)) {
        // vote round >= PBFT round
        db_->saveUnverifiedVote(vote);
        vote_mgr_->addUnverifiedVote(vote);
        onNewPbftVote(vote);
      }
    }
  } else if (pbft_currentpacket_rlpound == peer_pbftpacket_rlpound) {
    // Update previous round next votes
    const auto pbft_2t_plus_1 = db_->getPbft2TPlus1(pbft_currentpacket_rlpound - 1);
    if (pbft_2t_plus_1) {
      // Update our previous round next vote bundles...
      next_votes_mgr_->updateWithSyncedVotes(next_votes, pbft_2t_plus_1);
      // Pass them on to our peers...
      boost::shared_lock<boost::shared_mutex> lock(peers_state_->peers_mutex_);
      const auto updated_next_votes_size = next_votes_mgr_->getNextVotesSize();
      for (auto const &peer_to_share_to : peers_state_->peers_) {
        // Do not send votes right back to same peer...
        if (peer_to_share_to.first == packet_data.from_node_id_) continue;
        // Do not send votes to nodes that already have as many bundles as we do...
        if (peer_to_share_to.second->pbft_previous_round_next_votes_size_ >= updated_next_votes_size) continue;
        // Nodes may vote at different values at previous round, so need less or equal
        if (!peer_to_share_to.second->syncing_ && peer_to_share_to.second->pbft_round_ <= pbft_currentpacket_rlpound) {
          std::vector<Vote> send_next_votes_bundle;
          for (auto const &v : next_votes) {
            if (!peer_to_share_to.second->isVoteKnown(v.getHash())) {
              send_next_votes_bundle.emplace_back(v);
            }
          }
          sendPbftNextVotes(peer_to_share_to.first, send_next_votes_bundle);
        }
      }
    } else {
      LOG(log_er_) << "Cannot get PBFT 2t+1 in PBFT round " << pbft_currentpacket_rlpound - 1;
    }
  }
}

void VotePacketsHandler::sendPbftVote(NodeID const &peer_id, Vote const &vote) {
  const auto peer = peers_state_->getPeer(peer_id);
  // TODO: We should disable PBFT votes when a node is bootstrapping but not when trying to resync
  if (peer) {
    if (peers_state_->sealAndSend(peer_id, PbftVotePacket, RLPStream(1) << vote.rlp(true))) {
      LOG(log_dg_) << "sendPbftVote " << vote.getHash() << " to " << peer_id;
      peer->markVoteAsKnown(vote.getHash());
    }
  }
}

void VotePacketsHandler::onNewPbftVote(Vote const &vote) {
  std::vector<NodeID> peers_to_send;
  {
    boost::shared_lock<boost::shared_mutex> lock(peers_state_->peers_mutex_);
    for (auto const &peer : peers_state_->peers_) {
      if (!peer.second->isVoteKnown(vote.getHash())) {
        peers_to_send.push_back(peer.first);
      }
    }
  }
  for (auto const &peer_id : peers_to_send) {
    sendPbftVote(peer_id, vote);
  }
}

void VotePacketsHandler::sendPbftNextVotes(NodeID const &peer_id, std::vector<Vote> const &send_next_votes_bundle) {
  if (send_next_votes_bundle.empty()) {
    return;
  }

  RLPStream s(send_next_votes_bundle.size());
  for (auto const &next_vote : send_next_votes_bundle) {
    s.appendRaw(next_vote.rlp());
    LOG(log_dg_) << "Send out next vote " << next_vote.getHash() << " to peer " << peer_id;
  }

  if (peers_state_->sealAndSend(peer_id, PbftNextVotesPacket, move(s))) {
    LOG(log_nf_) << "Send out size of " << send_next_votes_bundle.size() << " PBFT next votes to " << peer_id;
    if (auto peer = peers_state_->getPeer(peer_id)) {
      for (auto const &v : send_next_votes_bundle) {
        peer->markVoteAsKnown(v.getHash());
      }
    }
  }
}

}  // namespace taraxa::network::tarcap