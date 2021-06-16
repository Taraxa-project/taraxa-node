#include "network/tarcap/packets_handlers/vote_packets_handler.hpp"

#include "consensus/pbft_manager.hpp"
#include "consensus/vote.hpp"

namespace taraxa::network::tarcap {

VotePacketsHandler::VotePacketsHandler(std::shared_ptr<PeersState> peers_state,
                                       std::shared_ptr<PacketsStats> packets_stats,
                                       std::shared_ptr<PbftManager> pbft_mgr, std::shared_ptr<VoteManager> vote_mgr,
                                       std::shared_ptr<NextVotesForPreviousRound> next_votes_mgr,
                                       std::shared_ptr<DbStorage> db, const addr_t &node_addr)
    : PacketHandler(std::move(peers_state), std::move(packets_stats), node_addr, "VOTES_PH"),
      pbft_mgr_(std::move(pbft_mgr)),
      vote_mgr_(std::move(vote_mgr)),
      next_votes_mgr_(std::move(next_votes_mgr)),
      db_(std::move(db)) {}

void VotePacketsHandler::process(const PacketData &packet_data, const std::shared_ptr<TaraxaPeer> &peer) {
  if (packet_data.type_ == SubprotocolPacketType::PbftVotePacket) {
    processPbftVotePacket(packet_data, peer);
  } else if (packet_data.type_ == SubprotocolPacketType::GetPbftNextVotes) {
    processGetPbftNextVotePacket(packet_data, peer);
  } else if (packet_data.type_ == SubprotocolPacketType::PbftNextVotesPacket) {
    processPbftNextVotesPacket(packet_data, peer);
  } else {
    assert(false);
  }
}

inline void VotePacketsHandler::processPbftVotePacket(const PacketData &packet_data,
                                                      const std::shared_ptr<TaraxaPeer> &peer) {
  auto vote = std::make_shared<Vote>(packet_data.rlp_[0].toBytes());
  const auto vote_hash = vote->getHash();
  LOG(log_dg_) << "Received PBFT vote " << vote_hash;

  const auto vote_round = vote->getRound();

  if (vote_round < pbft_mgr_->getPbftRound()) {
    LOG(log_dg_) << "Received old PBFT vote " << vote_hash << " from " << packet_data.from_node_id_.abridged()
                 << ". Vote round: " << vote_round << ", current pbft round: " << pbft_mgr_->getPbftRound();
    return;
  }

  peer->markVoteAsKnown(vote_hash);

  if (vote_mgr_->voteInUnverifiedMap(vote_round, vote_hash) || vote_mgr_->voteInVerifiedMap(vote)) {
    LOG(log_dg_) << "Received PBFT vote " << vote_hash << " (from " << packet_data.from_node_id_.abridged()
                 << ") already saved in queue.";
    return;
  }

  // Synchronization point in case multiple threads are processing the same vote at the same time
  // Adds unverified vote into local structure + database
  if (!vote_mgr_->addUnverifiedVote(vote)) {
    LOG(log_dg_) << "Received PBFT vote " << vote_hash << " (from " << packet_data.from_node_id_.abridged()
                 << ") already saved in unverified queue by a different thread(race condition).";
    return;
  }

  onNewPbftVote(vote);
}

inline void VotePacketsHandler::processGetPbftNextVotePacket(const PacketData &packet_data,
                                                             const std::shared_ptr<TaraxaPeer> &peer) {
  LOG(log_dg_) << "Received GetPbftNextVotes request";

  const uint64_t peer_pbft_round = packet_data.rlp_[0].toPositiveInt64();
  const size_t peer_pbft_previous_round_next_votes_size = packet_data.rlp_[1].toInt<unsigned>();
  const uint64_t pbft_round = pbft_mgr_->getPbftRound();
  const size_t pbft_previous_round_next_votes_size = next_votes_mgr_->getNextVotesSize();

  if (pbft_round > peer_pbft_round || (pbft_round == peer_pbft_round && pbft_previous_round_next_votes_size >
                                                                            peer_pbft_previous_round_next_votes_size)) {
    LOG(log_dg_) << "Current PBFT round is " << pbft_round << " previous round next votes size "
                 << pbft_previous_round_next_votes_size << ", and peer PBFT round is " << peer_pbft_round
                 << " previous round next votes size " << peer_pbft_previous_round_next_votes_size
                 << ". Will send out bundle of next votes";

    auto next_votes_bundle = next_votes_mgr_->getNextVotes();
    std::vector<std::shared_ptr<Vote>> send_next_votes_bundle;
    for (auto const &v : next_votes_bundle) {
      if (!peer->isVoteKnown(v->getHash())) {
        send_next_votes_bundle.push_back(std::move(v));
      }
    }
    sendPbftNextVotes(packet_data.from_node_id_, send_next_votes_bundle);
  }
}

inline void VotePacketsHandler::processPbftNextVotesPacket(const PacketData &packet_data,
                                                           const std::shared_ptr<TaraxaPeer> &peer) {
  const auto next_votes_count = packet_data.rlp_.itemCount();
  if (next_votes_count == 0) {
    LOG(log_er_) << "Receive 0 next votes from peer " << packet_data.from_node_id_
                 << ". The peer may be a malicous player, will be disconnected";
    disconnect(packet_data.from_node_id_, p2p::UserReason);
    return;
  }

  auto vote = std::make_shared<Vote>(packet_data.rlp_[0].data().toBytes());

  const auto pbft_currentpacket_rlpound = pbft_mgr_->getPbftRound();
  const auto peer_pbftpacket_rlpound = vote->getRound() + 1;

  std::vector<std::shared_ptr<Vote>> next_votes;
  for (size_t i = 0; i < next_votes_count; i++) {
    auto next_vote = std::make_shared<Vote>(packet_data.rlp_[i].data().toBytes());
    if (next_vote->getRound() != peer_pbftpacket_rlpound - 1) {
      LOG(log_er_) << "Received next votes bundle with unmatched rounds from " << packet_data.from_node_id_
                   << ". The peer may be a malicous player, will be disconnected";
      disconnect(packet_data.from_node_id_, p2p::UserReason);
      return;
    }

    const auto next_vote_hash = next_vote->getHash();
    LOG(log_nf_) << "Received PBFT next vote " << next_vote_hash;
    peer->markVoteAsKnown(next_vote_hash);
    next_votes.emplace_back(next_vote);
  }

  LOG(log_nf_) << "Received " << next_votes_count << " next votes from peer " << packet_data.from_node_id_
               << " node current round " << pbft_currentpacket_rlpound << ", peer pbft round "
               << peer_pbftpacket_rlpound;

  if (pbft_currentpacket_rlpound < peer_pbftpacket_rlpound) {
    // Add into votes unverified queue
    for (auto const &vote_n : next_votes) {
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

      onNewPbftVote(vote_n);
    }
  } else if (pbft_currentpacket_rlpound == peer_pbftpacket_rlpound) {
    // Update previous round next votes
    const auto pbft_2t_plus_1 = db_->getPbft2TPlus1(pbft_currentpacket_rlpound - 1);
    if (!pbft_2t_plus_1) {
      LOG(log_er_) << "Cannot get PBFT 2t+1 in PBFT round " << pbft_currentpacket_rlpound - 1;
      return;
    }

    // Update our previous round next vote bundles...
    next_votes_mgr_->updateWithSyncedVotes(next_votes, pbft_2t_plus_1);
    // Pass them on to our peers...
    const auto updated_next_votes_size = next_votes_mgr_->getNextVotesSize();
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
      if (!peer_to_share_to.second->syncing_ && peer_to_share_to.second->pbft_round_ > pbft_currentpacket_rlpound) {
        continue;
      }

      std::vector<std::shared_ptr<Vote>> send_next_votes_bundle;
      for (auto const &v : next_votes) {
        if (!peer_to_share_to.second->isVoteKnown(v->getHash())) {
          send_next_votes_bundle.push_back(std::move(v));
        }
      }
      sendPbftNextVotes(peer_to_share_to.first, send_next_votes_bundle);
    }
  }
}

void VotePacketsHandler::sendPbftVote(dev::p2p::NodeID const &peer_id, std::shared_ptr<Vote> const &vote) {
  const auto peer = peers_state_->getPeer(peer_id);
  // TODO: We should disable PBFT votes when a node is bootstrapping but not when trying to resync
  if (peer) {
    if (sealAndSend(peer_id, PbftVotePacket, std::move(RLPStream(1) << vote->rlp(true)))) {
      LOG(log_dg_) << "sendPbftVote " << vote->getHash() << " to " << peer_id;
      peer->markVoteAsKnown(vote->getHash());
    }
  }
}

void VotePacketsHandler::onNewPbftVote(std::shared_ptr<Vote> const &vote) {
  std::vector<dev::p2p::NodeID> peers_to_send;
  for (auto const &peer : peers_state_->getAllPeers()) {
    if (!peer.second->isVoteKnown(vote->getHash())) {
      peers_to_send.push_back(peer.first);
    }
  }
  for (auto const &peer_id : peers_to_send) {
    sendPbftVote(peer_id, vote);
  }
}

void VotePacketsHandler::sendPbftNextVotes(dev::p2p::NodeID const &peer_id,
                                           std::vector<std::shared_ptr<Vote>> const &send_next_votes_bundle) {
  if (send_next_votes_bundle.empty()) {
    return;
  }

  RLPStream s(send_next_votes_bundle.size());
  for (auto const &next_vote : send_next_votes_bundle) {
    s.appendRaw(next_vote->rlp());
    LOG(log_dg_) << "Send out next vote " << next_vote->getHash() << " to peer " << peer_id;
  }

  if (sealAndSend(peer_id, PbftNextVotesPacket, move(s))) {
    LOG(log_nf_) << "Send out size of " << send_next_votes_bundle.size() << " PBFT next votes to " << peer_id;
    if (auto peer = peers_state_->getPeer(peer_id)) {
      for (auto const &v : send_next_votes_bundle) {
        peer->markVoteAsKnown(v->getHash());
      }
    }
  }
}

void VotePacketsHandler::broadcastPreviousRoundNextVotesBundle() {
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
