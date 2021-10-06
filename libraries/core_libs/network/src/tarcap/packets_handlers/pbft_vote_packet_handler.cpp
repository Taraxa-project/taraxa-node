#include "network/tarcap/packets_handlers/pbft_vote_packet_handler.hpp"

#include "pbft/pbft_manager.hpp"
#include "vote_manager/vote_manager.hpp"

namespace taraxa::network::tarcap {

PbftVotePacketHandler::PbftVotePacketHandler(std::shared_ptr<PeersState> peers_state,
                                             std::shared_ptr<PacketsStats> packets_stats,
                                             std::shared_ptr<PbftManager> pbft_mgr,
                                             std::shared_ptr<VoteManager> vote_mgr, const addr_t &node_addr)
    : ExtVotesPacketHandler(std::move(peers_state), std::move(packets_stats), node_addr, "PBFT_VOTE_PH"),
      pbft_mgr_(std::move(pbft_mgr)),
      vote_mgr_(std::move(vote_mgr)) {}

void PbftVotePacketHandler::process(const PacketData &packet_data, const std::shared_ptr<TaraxaPeer> &peer) {
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

}  // namespace taraxa::network::tarcap
