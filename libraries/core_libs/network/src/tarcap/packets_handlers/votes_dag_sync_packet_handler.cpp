#include "network/tarcap/packets_handlers/votes_dag_sync_packet_handler.hpp"

#include "final_chain/final_chain.hpp"
#include "pbft/pbft_chain.hpp"
#include "pbft/pbft_manager.hpp"
#include "votes/rewards_votes.hpp"
#include "votes/vote_manager.hpp"

namespace taraxa::network::tarcap {

VotesDagSyncPacketHandler::VotesDagSyncPacketHandler(
    std::shared_ptr<PeersState> peers_state, std::shared_ptr<PacketsStats> packets_stats,
    std::shared_ptr<PbftManager> pbft_mgr, std::shared_ptr<VoteManager> vote_mgr,
    std::shared_ptr<final_chain::FinalChain> final_chain, std::shared_ptr<PbftChain> pbft_chain,
    std::shared_ptr<DbStorage> db, std::shared_ptr<RewardsVotes> rewards_votes, const addr_t &node_addr)
    : ExtVotesPacketHandler(std::move(peers_state), std::move(packets_stats), node_addr, "VOTES_DAG_SYNC_PH"),
      pbft_mgr_(std::move(pbft_mgr)),
      vote_mgr_(std::move(vote_mgr)),
      final_chain_(std::move(final_chain)),
      pbft_chain_(std::move(pbft_chain)),
      rewards_votes_(std::move(rewards_votes)),
      db_(std::move(db)) {}

void VotesDagSyncPacketHandler::process(const PacketData &packet_data, const std::shared_ptr<TaraxaPeer> &peer) {
  const auto votes_count = packet_data.rlp_.itemCount();
  if (votes_count == 0) {
    LOG(log_er_) << "Received 0 Dag sync votes from peer " << packet_data.from_node_id_
                 << ". The peer may be a malicIous player, will be disconnected";
    disconnect(packet_data.from_node_id_, dev::p2p::UserReason);
    return;
  }

  // Process each vote
  for (size_t i = 0; i < votes_count; i++) {
    auto vote = std::make_shared<Vote>(packet_data.rlp_[i].data().toBytes());

    // Vote was already processed
    if (rewards_votes_->isKnownVote(vote)) {
      continue;
    }

    // Checks if received vote is cert votes
    if (vote->getType() != PbftVoteTypes::cert_vote_type) {
      LOG(log_er_) << "Received dag sync votes bundle contains non-cert votes. Peer " << packet_data.from_node_id_
                   << " may be a malicious player, will be disconnected";
      disconnect(packet_data.from_node_id_, dev::p2p::UserReason);
      return;
    }

    const auto vote_hash = vote->getHash();
    LOG(log_dg_) << "Received dag sync vote " << vote_hash;
    peer->markVoteAsKnown(vote_hash);

    // Check if the voter account is valid, malicious vote
    addr_t voter = vote->getVoterAddr();
    auto vote_weighted_index = vote->getWeightedIndex();
    size_t voter_dpos_votes_count = 0;

    //    // get voter dpos eligible vote count
    //    try {
    //      voter_dpos_votes_count = final_chain_->dpos_eligible_vote_count(current_pbft_period - 1, voter);
    //    } catch (state_api::ErrFutureBlock &c) {
    //      LOG(log_er_) << c.what() << ". Period " << current_pbft_period - 1
    //                   << " is too far ahead of DPOS, have executed chain size " << final_chain_->last_block_number();
    //      // TODO: should all these failed checks do continue or return ?
    //      continue;
    //    }

    if (vote_weighted_index >= voter_dpos_votes_count) {
      LOG(log_dg_) << "Account " << voter << " was not eligible to vote. Vote: " << vote;
      continue;
    }

    // TODO: pbft_mgr_->getPreviousPbftPeriodDposTotalVotesCount and
    // pbft_mgr_->getPreviousPbftPeriodSortitionThreshold() mght be incosistent with current_pbft_period - 1
    if (auto result = vote->validate(pbft_mgr_->getPreviousPbftPeriodDposTotalVotesCount(),
                                     pbft_mgr_->getPreviousPbftPeriodSortitionThreshold());
        !result.first) {
      LOG(log_dg_) << "Account " << voter << " was not eligible to vote. Reason: " << result.second
                   << ", Vote: " << vote;
      continue;
    }

    // Inserts vote into the list of votes that were not part of the latest finalized pbft block observed 2t+1 cert
    // votes
    rewards_votes_->insertVote(std::move(vote));
  }
}

}  // namespace taraxa::network::tarcap
