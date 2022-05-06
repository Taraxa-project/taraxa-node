#include "network/tarcap/packets_handlers/vote_packet_handler.hpp"

#include "pbft/pbft_manager.hpp"
#include "vote_manager/vote_manager.hpp"

namespace taraxa::network::tarcap {

VotePacketHandler::VotePacketHandler(std::shared_ptr<PeersState> peers_state,
                                     std::shared_ptr<PacketsStats> packets_stats, std::shared_ptr<PbftManager> pbft_mgr,
                                     std::shared_ptr<VoteManager> vote_mgr, const addr_t &node_addr)
    : ExtVotesPacketHandler(std::move(peers_state), std::move(packets_stats), node_addr, "PBFT_VOTE_PH"),
      pbft_mgr_(std::move(pbft_mgr)),
      vote_mgr_(std::move(vote_mgr)),
      seen_votes_(1000000, 1000) {}

void VotePacketHandler::validatePacketRlpFormat([[maybe_unused]] const PacketData &packet_data) const {
  // Number of votes is not fixed, nothing to be checked here
}

void VotePacketHandler::process(const PacketData &packet_data, const std::shared_ptr<TaraxaPeer> &peer) {
  const auto count = packet_data.rlp_.itemCount();
  if (count == 0 || count > kMaxVotesInPacket) {
    std::ostringstream err_msg;
    err_msg << "Receive " << count << " votes from peer " << packet_data.from_node_id_
            << ". The peer is a malicious player, will be disconnected";
    throw MaliciousPeerException(err_msg.str());
  }

  std::vector<std::shared_ptr<Vote>> votes;
  for (size_t i = 0; i < count; i++) {
    auto vote = std::make_shared<Vote>(packet_data.rlp_[i].toBytes());
    const auto vote_hash = vote->getHash();
    LOG(log_dg_) << "Received PBFT vote " << vote_hash;

    const auto vote_round = vote->getRound();

    if (vote_round < pbft_mgr_->getPbftRound()) {
      LOG(log_dg_) << "Received old PBFT vote " << vote_hash << " from " << packet_data.from_node_id_.abridged()
                   << ". Vote round: " << vote_round << ", current pbft round: " << pbft_mgr_->getPbftRound();
      continue;
    }

    // Synchronization point in case multiple threads are processing the same vote at the same time
    if (!seen_votes_.insert(vote_hash)) {
      LOG(log_dg_) << "Received PBFT vote " << vote_hash << " (from " << packet_data.from_node_id_.abridged()
                   << ") already seen.";
      continue;
    }

    // Adds unverified vote into queue
    if (vote_mgr_->voteInVerifiedMap(vote) || !vote_mgr_->addUnverifiedVote(vote)) {
      LOG(log_dg_) << "Received PBFT vote " << vote_hash << " (from " << packet_data.from_node_id_.abridged()
                   << ") already saved in (un)verified queues.";
      continue;
    }

    // Do not push it before, as peers have small caches of known votes. Only push gossiping votes
    votes.push_back(std::move(vote));
  }

  for (const auto &v : votes) {
    peer->markVoteAsKnown(v->getHash());
  }

  onNewPbftVotes(std::move(votes));
}

}  // namespace taraxa::network::tarcap
