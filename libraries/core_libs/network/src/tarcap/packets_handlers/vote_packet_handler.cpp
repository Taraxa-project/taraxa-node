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

  std::shared_ptr<Vote> vote = nullptr;

  std::shared_ptr<Vote> single_vote_with_block = nullptr;
  std::shared_ptr<PbftBlock> pbft_block = nullptr;
  uint64_t peer_chain_size = 0;

  std::vector<std::shared_ptr<Vote>> votes;
  std::vector<std::shared_ptr<Vote>> previous_next_votes;
  for (size_t i = 0; i < packet_data.rlp_.itemCount(); i++) {
    // TODO[2031]: the way we differentiate now between receiving single vote with some optional data vs
    //             batch of simple votes is ugly now. We could change the way we use VotePacket vs VotesSyncPacket to
    //             make it more clear
    // There is included also PBFT block as optional value in Vote packet
    if (packet_data.rlp_.itemCount() == 1 && packet_data.rlp_[0].itemCount() == kVotePacketWithBlockSize &&
        packet_data.rlp_[0][0].isList()) {
      // We allow only single votes to be sent together with block in a bundle(1 item)
      const auto &vote_with_block_rlp = packet_data.rlp_[0];
      if (vote_with_block_rlp.itemCount() != kVotePacketWithBlockSize) {
        LOG(log_er_) << "Vote packet with included PBFT block invalid items count: " << vote_with_block_rlp.itemCount();
        throw InvalidRlpItemsCountException(packet_data.type_str_, vote_with_block_rlp.itemCount(),
                                            kVotePacketWithBlockSize);
      }

      vote = std::make_shared<Vote>(vote_with_block_rlp[0]);
      pbft_block = std::make_shared<PbftBlock>(vote_with_block_rlp[1]);
      peer_chain_size = vote_with_block_rlp[2].toInt();
      single_vote_with_block = vote;

      if (pbft_block->getBlockHash() != vote->getBlockHash()) {
        std::ostringstream err_msg;
        err_msg << "Vote " << vote->getHash().abridged() << " voted block " << vote->getBlockHash().abridged()
                << " != actual block " << pbft_block->getBlockHash().abridged();
        throw MaliciousPeerException(err_msg.str());
      }

      peer->markPbftBlockAsKnown(pbft_block->getBlockHash());
      LOG(log_dg_) << "Received PBFT vote " << vote->getHash() << " with PBFT block " << pbft_block->getBlockHash();
    } else {
      vote = std::make_shared<Vote>(packet_data.rlp_[i]);
      pbft_block = nullptr;
      peer_chain_size = 0;
      LOG(log_dg_) << "Received PBFT vote " << vote->getHash();
    }

    const auto vote_hash = vote->getHash();

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
      if (!processNextSyncVote(vote, pbft_block)) {
        continue;
      }

      // Not perfect way to to do this, but this whole process could be possibly refactored
      previous_next_votes.push_back(vote);

    } else if (vote->getPeriod() >= current_pbft_period) {
      // Standard vote
      if (!processStandardVote(vote, pbft_block, peer, true)) {
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

  if (single_vote_with_block && !votes.empty()) {
    onNewPbftVote(single_vote_with_block, pbft_block);
    // Update peer's max chain size
    if (peer_chain_size > peer->pbft_chain_size_) {
      peer->pbft_chain_size_ = peer_chain_size;
    }
  } else {
    onNewPbftVotes(std::move(votes));
  }
}

}  // namespace taraxa::network::tarcap
