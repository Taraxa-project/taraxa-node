#include "network/tarcap/packets_handlers/vote_packet_handler.hpp"

#include "pbft/pbft_manager.hpp"
#include "vote_manager/vote_manager.hpp"

namespace taraxa::network::tarcap {

VotePacketHandler::VotePacketHandler(std::shared_ptr<PeersState> peers_state,
                                     std::shared_ptr<PacketsStats> packets_stats, std::shared_ptr<PbftManager> pbft_mgr,
                                     std::shared_ptr<VoteManager> vote_mgr, const addr_t &node_addr)
    : ExtVotesPacketHandler(std::move(peers_state), std::move(packets_stats), pbft_mgr, node_addr, "PBFT_VOTE_PH"),
      pbft_mgr_(std::move(pbft_mgr)),
      vote_mgr_(std::move(vote_mgr)),
      seen_votes_(1000000, 1000) {}

void VotePacketHandler::validatePacketRlpFormat([[maybe_unused]] const PacketData &packet_data) const {
  auto items = packet_data.rlp_.itemCount();
  if (items == 0 || items > kMaxVotesInPacket) {
    throw InvalidRlpItemsCountException(packet_data.type_str_, items, kMaxVotesInPacket);
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
    const auto current_pbft_round = pbft_mgr_->getPbftRound();

    // Check reward vote
    if (vote_round < current_pbft_round) {
      // Synchronization point in case multiple threads are processing the same vote at the same time
      if (!seen_votes_.insert(vote_hash)) {
        LOG(log_dg_) << "Received vote " << vote_hash << " (from " << packet_data.from_node_id_.abridged()
                     << ") already seen.";
      } else if (vote_mgr_->addRewardVote(vote)) {
        // As peers have small caches of known votes. Only mark gossiping votes
        peer->markVoteAsKnown(vote_hash);
        votes.push_back(std::move(vote));
      }
      continue;
    }

    // Votes vote_round >= current_pbft_round
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
