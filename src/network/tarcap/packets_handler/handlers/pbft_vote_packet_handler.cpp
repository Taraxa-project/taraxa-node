#include "pbft_vote_packet_handler.hpp"

#include "consensus/pbft_chain.hpp"
#include "consensus/pbft_manager.hpp"
#include "consensus/vote.hpp"
#include "network/tarcap/packets_handler/syncing_state.hpp"

namespace taraxa::network::tarcap {

PbftVotePacketHandler::PbftVotePacketHandler(std::shared_ptr<PeersState> peers_state,
                                             std::shared_ptr<SyncingState> syncing_state,
                                             std::shared_ptr<PbftChain> pbft_chain,
                                             std::shared_ptr<PbftManager> pbft_mgr,
                                             std::shared_ptr<VoteManager> vote_mgr, std::shared_ptr<DbStorage> db,
                                             const addr_t &node_addr)
    : PacketHandler(std::move(peers_state), node_addr, "PBFT_VOTE_PH"),
      syncing_state_(syncing_state),
      pbft_chain_(pbft_chain),
      pbft_mgr_(pbft_mgr),
      vote_mgr_(vote_mgr),
      db_(db) {}

void PbftVotePacketHandler::process(const PacketData & /*packet_data*/, const dev::RLP &packet_rlp) {
  LOG(log_dg_) << "In PbftVotePacket";

  Vote vote(packet_rlp[0].toBytes(), false);
  auto vote_hash = vote.getHash();
  LOG(log_dg_) << "Received PBFT vote " << vote_hash;

  auto pbft_round = pbft_mgr_->getPbftRound();
  auto vote_round = vote.getRound();

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

void PbftVotePacketHandler::sendPbftVote(NodeID const &peerID, Vote const &vote) {
  auto peer = peers_state_->getPeer(peerID);
  // TODO: We should disable PBFT votes when a node is bootstrapping but not when trying to resync
  if (peer) {
    if (peers_state_->sealAndSend(peerID, PbftVotePacket, RLPStream(1) << vote.rlp(true))) {
      LOG(log_dg_) << "sendPbftVote " << vote.getHash() << " to " << peerID;
      peer->markVoteAsKnown(vote.getHash());
    }
  }
}

void PbftVotePacketHandler::onNewPbftVote(Vote const &vote) {
  std::vector<NodeID> peers_to_send;
  {
    boost::shared_lock<boost::shared_mutex> lock(peers_state_->peers_mutex_);
    for (auto const &peer : peers_state_->peers_) {
      if (!peer.second->isVoteKnown(vote.getHash())) {
        peers_to_send.push_back(peer.first);
      }
    }
  }
  for (auto const &peerID : peers_to_send) {
    sendPbftVote(peerID, vote);
  }
}

}  // namespace taraxa::network::tarcap