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

void VotePacketHandler::validatePacketRlpFormat(const PacketData &packet_data) {
  checkPacketRlpList(packet_data);

  if (size_t required_size = 1; packet_data.rlp_.itemCount() != required_size) {
    throw InvalidRlpItemsCountException(packet_data.type_str_, packet_data.rlp_.itemCount(), required_size);
  }

  // In case there is a type mismatch, one of the dev::RLPException's is thrown during further parsing
}

void VotePacketHandler::process(const PacketData &packet_data, const std::shared_ptr<TaraxaPeer> &peer) {
  auto vote = std::make_shared<Vote>(packet_data.rlp_[0].toBytes());
  const auto vote_hash = vote->getHash();
  LOG(log_dg_) << "Received PBFT vote " << vote_hash;

  const auto vote_round = vote->getRound();

  if (vote_round < pbft_mgr_->getPbftRound()) {
    LOG(log_dg_) << "Received old PBFT vote " << vote_hash << " from " << packet_data.from_node_id_.abridged()
                 << ". Vote round: " << vote_round << ", current pbft round: " << pbft_mgr_->getPbftRound();
    return;
  }

  // Synchronization point in case multiple threads are processing the same vote at the same time
  if (!seen_votes_.insert(vote_hash)) {
    LOG(log_dg_) << "Received PBFT vote " << vote_hash << " (from " << packet_data.from_node_id_.abridged()
                 << ") already seen.";
    return;
  }

  // Adds unverified vote into local structure
  if (vote_mgr_->voteInVerifiedMap(vote) || !vote_mgr_->addUnverifiedVote(vote)) {
    LOG(log_dg_) << "Received PBFT vote " << vote_hash << " (from " << packet_data.from_node_id_.abridged()
                 << ") already saved in (un)verified queues.";
    return;
  }

  // Do not mark it before, as peers have small caches of known votes.
  // And we do not need to mark it before this point as we won't be sending
  peer->markVoteAsKnown(vote_hash);

  onNewPbftVote(std::move(vote));
}

}  // namespace taraxa::network::tarcap
