#include "network/tarcap/packets_handlers/common/ext_votes_packet_handler.hpp"

#include "vote/vote.hpp"

namespace taraxa::network::tarcap {

ExtVotesPacketHandler::ExtVotesPacketHandler(std::shared_ptr<PeersState> peers_state,
                                             std::shared_ptr<PacketsStats> packets_stats, const addr_t &node_addr,
                                             const std::string &log_channel_name)
    : PacketHandler(std::move(peers_state), std::move(packets_stats), node_addr, log_channel_name) {}

void ExtVotesPacketHandler::onNewPbftVote(std::shared_ptr<Vote> const &vote) {
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

void ExtVotesPacketHandler::rebroadcastOwnNextVote(const std::shared_ptr<Vote> &vote) {
  std::vector<dev::p2p::NodeID> peers_to_send;
  const auto vote_round = vote->getRound();
  for (auto const &peer : peers_state_->getAllPeers()) {
    // only send to peer with the same PBFT round
    if (!peer.second->isVoteKnown(vote->getHash()) && peer.second->pbft_round_ == vote_round) {
      peers_to_send.push_back(peer.first);
    }
  }
  for (auto const &peer_id : peers_to_send) {
    sendPbftVote(peer_id, vote);
  }
}

void ExtVotesPacketHandler::sendPbftVote(dev::p2p::NodeID const &peer_id, std::shared_ptr<Vote> const &vote) {
  const auto peer = peers_state_->getPeer(peer_id);
  // TODO: We should disable PBFT votes when a node is bootstrapping but not when trying to resync
  if (peer) {
    if (sealAndSend(peer_id, SubprotocolPacketType::VotePacket, std::move(dev::RLPStream(1) << vote->rlp(true)))) {
      LOG(log_dg_) << "sendPbftVote " << vote->getHash() << " to " << peer_id;
      peer->markVoteAsKnown(vote->getHash());
    }
  }
}

void ExtVotesPacketHandler::sendPbftNextVotes(dev::p2p::NodeID const &peer_id,
                                              std::vector<std::shared_ptr<Vote>> const &send_next_votes_bundle) {
  if (send_next_votes_bundle.empty()) {
    return;
  }

  dev::RLPStream s(send_next_votes_bundle.size());
  for (auto const &next_vote : send_next_votes_bundle) {
    s.appendRaw(next_vote->rlp());
    LOG(log_dg_) << "Send out next vote " << next_vote->getHash() << " to peer " << peer_id;
  }

  if (sealAndSend(peer_id, SubprotocolPacketType::VotesSyncPacket, std::move(s))) {
    LOG(log_nf_) << "Send out size of " << send_next_votes_bundle.size() << " PBFT next votes to " << peer_id;
    if (auto peer = peers_state_->getPeer(peer_id)) {
      for (auto const &v : send_next_votes_bundle) {
        peer->markVoteAsKnown(v->getHash());
      }
    }
  }
}

}  // namespace taraxa::network::tarcap
