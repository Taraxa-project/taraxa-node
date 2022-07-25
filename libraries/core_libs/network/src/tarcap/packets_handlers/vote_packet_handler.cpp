#include "network/tarcap/packets_handlers/vote_packet_handler.hpp"

#include "pbft/pbft_manager.hpp"
#include "vote_manager/vote_manager.hpp"

namespace taraxa::network::tarcap {

VotePacketHandler::VotePacketHandler(std::shared_ptr<PeersState> peers_state,
                                     std::shared_ptr<PacketsStats> packets_stats, std::shared_ptr<PbftManager> pbft_mgr,
                                     std::shared_ptr<PbftChain> pbft_chain, std::shared_ptr<VoteManager> vote_mgr,
                                     const addr_t &node_addr)
    : ExtVotesPacketHandler(std::move(peers_state), std::move(packets_stats), std::move(pbft_mgr),
                            std::move(pbft_chain), node_addr, "PBFT_VOTE_PH"),
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

    // Synchronization point in case multiple threads are processing the same vote at the same time
    // TODO: we might not need this as such synchronization is done in queues already ?
    if (!seen_votes_.insert(vote_hash)) {
      LOG(log_dg_) << "Received vote " << vote_hash << " (from " << packet_data.from_node_id_.abridged()
                   << ") already seen.";
      continue;
    }
    peer->markVoteAsKnown(vote_hash);

    // TODO: I am so sure about this, why do we allow smaller round without any bound ?
    // TODO: this is bad - we say vote is reward vote based on round, but it is not enough as some out of sync node
    // might send us standard vote and we identify as reward vote - this way we cant really set peer as malicious if
    // reward vote type != cert vote, etc...
    if (vote_round < current_pbft_round) {  // reward vote
      if (vote_mgr_->isKnownRewardVote(vote->getHash())) {
        LOG(log_dg_) << "Reward vote " << vote_hash.abridged() << " already inserted in verified queue";
      }

      if (auto vote_is_valid = validateRewardVote(vote); vote_is_valid.first == false) {
        LOG(log_er_) << "Reward vote " << vote_hash.abridged() << " validation failed. Err: " << vote_is_valid.second;
        continue;
      }

      if (!vote_mgr_->addRewardVote(vote)) {
        LOG(log_dg_) << "Reward vote " << vote_hash.abridged() << " already inserted in verified queue(race condition)";
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
    }

    // TODO: add protection so each voter can cast only 1 vote per round & step

    // Do not mark it before, as peers have small caches of known votes. Only mark gossiping votes
    peer->markVoteAsKnown(vote_hash);
    votes.push_back(std::move(vote));
  }

  onNewPbftVotes(std::move(votes));
}

}  // namespace taraxa::network::tarcap
