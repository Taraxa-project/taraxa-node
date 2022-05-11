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
  auto iteam_count = packet_data.rlp_.itemCount();
  if (iteam_count == 0 || iteam_count > kMaxVotesInPacket) {
    throw InvalidRlpItemsCountException(packet_data.type_str_, iteam_count, 1, kMaxVotesInPacket);
  }
}

void VotePacketHandler::process(const PacketData &packet_data, const std::shared_ptr<TaraxaPeer> &peer) {
  std::vector<std::shared_ptr<Vote>> votes;
  const auto count = packet_data.rlp_.itemCount();
  for (size_t i = 0; i < count; i++) {
    auto vote = std::make_shared<Vote>(packet_data.rlp_[i].data().toBytes());
    const auto vote_hash = vote->getHash();
    LOG(log_dg_) << "Received PBFT vote " << vote_hash;

    const auto vote_round = vote->getRound();
    const auto pbft_round = pbft_mgr_->getPbftRound();
    if (vote_round < pbft_round) {
      LOG(log_dg_) << "Received old PBFT vote " << vote_hash << " from " << packet_data.from_node_id_.abridged()
                   << ". Vote round: " << vote_round << ", current pbft round: " << pbft_round;
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

    // Do not mark it before, as peers have small caches of known votes. Only mark gossiping votes
    peer->markVoteAsKnown(vote_hash);
    votes.push_back(std::move(vote));
  }

  onNewPbftVotes(std::move(votes));
}

}  // namespace taraxa::network::tarcap
