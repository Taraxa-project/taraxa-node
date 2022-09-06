#include "network/tarcap/packets_handlers/vote_packet_handler.hpp"

#include "pbft/pbft_manager.hpp"
#include "vote_manager/vote_manager.hpp"

namespace taraxa::network::tarcap {

VotePacketHandler::VotePacketHandler(std::shared_ptr<PeersState> peers_state,
                                     std::shared_ptr<PacketsStats> packets_stats, std::shared_ptr<PbftManager> pbft_mgr,
                                     std::shared_ptr<PbftChain> pbft_chain, std::shared_ptr<VoteManager> vote_mgr,
                                     std::shared_ptr<NextVotesManager> next_vote_mgr, const NetworkConfig &net_config,
                                     const addr_t &node_addr)
    : ExtVotesPacketHandler(std::move(peers_state), std::move(packets_stats), std::move(pbft_mgr),
                            std::move(pbft_chain), std::move(vote_mgr), net_config, node_addr, "PBFT_VOTE_PH"),
      seen_votes_(1000000, 1000),
      next_votes_mgr_(next_vote_mgr) {}

void VotePacketHandler::validatePacketRlpFormat([[maybe_unused]] const PacketData &packet_data) const {
  auto items = packet_data.rlp_.itemCount();
  if (items == 0 || items > kMaxVotesInPacket) {
    throw InvalidRlpItemsCountException(packet_data.type_str_, items, kMaxVotesInPacket);
  }
}

void VotePacketHandler::process(const PacketData &packet_data, const std::shared_ptr<TaraxaPeer> &peer) {
  const auto [current_pbft_round, current_pbft_period] = pbft_mgr_->getPbftRoundAndPeriod();

  std::vector<std::shared_ptr<Vote>> votes;
  std::vector<std::shared_ptr<Vote>> previous_next_votes;
  const auto count = packet_data.rlp_.itemCount();
  for (size_t i = 0; i < count; i++) {
    auto vote = std::make_shared<Vote>(packet_data.rlp_[i].data().toBytes());
    const auto vote_hash = vote->getHash();
    LOG(log_dg_) << "Received PBFT vote " << vote_hash;

    // Synchronization point in case multiple threads are processing the same vote at the same time
    if (!seen_votes_.insert(vote_hash)) {
      LOG(log_dg_) << "Received vote " << vote_hash << " (from " << packet_data.from_node_id_.abridged()
                   << ") already seen.";
      continue;
    }

    if (vote->getPeriod() == current_pbft_period && (current_pbft_round - 1) == vote->getRound() &&
        vote->getType() == PbftVoteTypes::next_vote_type) {
      // Previous round next vote
      // We could switch round before other nodes, so we need to process also previous round next votes
      if (!processNextSyncVote(vote)) {
        continue;
      }

      // Not perfect way to to do this, but this whole process could be possibly refactored
      previous_next_votes.push_back(vote);

    } else if (vote->getPeriod() >= current_pbft_period) {
      // Standard vote
      if (!processStandardVote(vote, peer, true)) {
        continue;
      }

    } else if (vote->getPeriod() == current_pbft_period - 1 && vote->getType() == PbftVoteTypes::cert_vote_type) {
      // potential reward vote
      if (!processRewardVote(vote)) {
        continue;
      }

    } else {
      // Too old vote
      LOG(log_dg_) << "Drop vote " << vote_hash.abridged() << ". Vote period " << vote->getPeriod()
                   << " too old. current_pbft_period " << current_pbft_period;
      continue;
    }

    // Do not mark it before, as peers have small caches of known votes. Only mark gossiping votes
    peer->markVoteAsKnown(vote_hash);
    votes.push_back(std::move(vote));
  }

  if (!previous_next_votes.empty()) {
    next_votes_mgr_->updateWithSyncedVotes(previous_next_votes, pbft_mgr_->getTwoTPlusOne());
  }

  onNewPbftVotes(std::move(votes));
}

void VotePacketHandler::broadcastNextVotesBundle(const std::vector<std::shared_ptr<Vote>> &next_votes_bundle) {
  if (next_votes_bundle.empty()) {
    LOG(log_er_) << "There are empty next votes for previous PBFT round";
    return;
  }

  const auto pbft_current_round = pbft_mgr_->getPbftRound();

  for (auto const &peer : peers_state_->getAllPeers()) {
    // Nodes may vote at different values at previous round, so need less or equal
    if (!peer.second->syncing_ && peer.second->pbft_round_ <= pbft_current_round) {
      std::vector<std::shared_ptr<Vote>> send_next_votes_bundle;
      for (const auto &v : next_votes_bundle) {
        send_next_votes_bundle.push_back(v);
      }
      sendPbftVotes(peer.first, std::move(send_next_votes_bundle));
    }
  }
}

}  // namespace taraxa::network::tarcap
