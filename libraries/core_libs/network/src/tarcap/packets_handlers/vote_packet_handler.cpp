#include "network/tarcap/packets_handlers/vote_packet_handler.hpp"

#include "pbft/pbft_manager.hpp"
#include "vote_manager/vote_manager.hpp"

namespace taraxa::network::tarcap {

VotePacketHandler::VotePacketHandler(std::shared_ptr<PeersState> peers_state,
                                     std::shared_ptr<PacketsStats> packets_stats, std::shared_ptr<PbftManager> pbft_mgr,
                                     std::shared_ptr<PbftChain> pbft_chain, std::shared_ptr<VoteManager> vote_mgr,
                                     const uint32_t dpos_delay, const addr_t &node_addr)
    : ExtVotesPacketHandler(std::move(peers_state), std::move(packets_stats), std::move(pbft_mgr),
                            std::move(pbft_chain), std::move(vote_mgr), dpos_delay, node_addr, "PBFT_VOTE_PH"),
      seen_votes_(1000000, 1000) {}

void VotePacketHandler::validatePacketRlpFormat([[maybe_unused]] const PacketData &packet_data) const {
  auto items = packet_data.rlp_.itemCount();
  if (items == 0 || items > kMaxVotesInPacket) {
    throw InvalidRlpItemsCountException(packet_data.type_str_, items, kMaxVotesInPacket);
  }
}

void VotePacketHandler::process(const PacketData &packet_data, const std::shared_ptr<TaraxaPeer> &peer) {
  const auto current_pbft_round = pbft_mgr_->getPbftRound();

  std::vector<std::shared_ptr<Vote>> votes;
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
    peer->markVoteAsKnown(vote_hash);

    // TODO[1880]: We identify vote as reward vote based on round, but if some out of sync node sends us standard vote
    // we identify it as reward vote and use wrong validation....
    if (vote->getRound() < current_pbft_round) {  // reward vote
      if (vote_mgr_->isInRewardsVotes(vote->getHash())) {
        LOG(log_dg_) << "Reward vote " << vote_hash.abridged() << " already inserted in rewards votes";
      }

      if (auto vote_is_valid = validateRewardVote(vote); vote_is_valid.first == false) {
        LOG(log_er_) << "Reward vote " << vote_hash.abridged() << " validation failed. Err: \"" << vote_is_valid.second
                     << "\", vote round " << vote->getRound() << ", current round: " << current_pbft_round
                     << ", vote type: " << static_cast<uint64_t>(vote->getType());
        continue;
      }

      if (!vote_mgr_->addRewardVote(vote)) {
        LOG(log_dg_) << "Reward vote " << vote_hash.abridged() << " already inserted in reward votes(race condition)";
        continue;
      }
    } else {  // standard vote -> vote_round >= current_pbft_round
      if (vote_mgr_->voteInVerifiedMap(vote)) {
        LOG(log_dg_) << "Vote " << vote_hash.abridged() << " already inserted in verified queue";
      }

      if (auto vote_is_valid = validateStandardVote(vote); vote_is_valid.first == false) {
        LOG(log_er_) << "Vote " << vote_hash.abridged() << " validation failed. Err: " << vote_is_valid.second;
        continue;
      }

      if (!vote_mgr_->addVerifiedVote(vote)) {
        LOG(log_dg_) << "Vote " << vote_hash << " already inserted in verified queue(race condition)";
        continue;
      }

      setVoterMaxRound(vote->getVoterAddr(), vote->getRound());
    }

    // Do not mark it before, as peers have small caches of known votes. Only mark gossiping votes
    peer->markVoteAsKnown(vote_hash);
    votes.push_back(std::move(vote));
  }

  onNewPbftVotes(std::move(votes));
}

}  // namespace taraxa::network::tarcap
