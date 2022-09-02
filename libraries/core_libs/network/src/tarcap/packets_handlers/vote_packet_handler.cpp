#include "network/tarcap/packets_handlers/vote_packet_handler.hpp"

#include "pbft/pbft_manager.hpp"
#include "vote_manager/vote_manager.hpp"

namespace taraxa::network::tarcap {

VotePacketHandler::VotePacketHandler(std::shared_ptr<PeersState> peers_state,
                                     std::shared_ptr<PacketsStats> packets_stats, std::shared_ptr<PbftManager> pbft_mgr,
                                     std::shared_ptr<PbftChain> pbft_chain, std::shared_ptr<VoteManager> vote_mgr,
                                     const NetworkConfig &net_config, const addr_t &node_addr)
    : ExtVotesPacketHandler(std::move(peers_state), std::move(packets_stats), std::move(pbft_mgr),
                            std::move(pbft_chain), std::move(vote_mgr), net_config.vote_accepting_periods, node_addr,
                            "PBFT_VOTE_PH"),
      kVoteAcceptingRounds(net_config.vote_accepting_rounds),
      kVoteAcceptingSteps(net_config.vote_accepting_steps),
      seen_votes_(1000000, 1000) {}

void VotePacketHandler::validatePacketRlpFormat([[maybe_unused]] const PacketData &packet_data) const {
  auto items = packet_data.rlp_.itemCount();
  if (items == 0 || items > kMaxVotesInPacket) {
    throw InvalidRlpItemsCountException(packet_data.type_str_, items, kMaxVotesInPacket);
  }
}

bool VotePacketHandler::checkVoteMaxPeriodRoundStep(const std::shared_ptr<Vote> &vote,
                                                    const std::shared_ptr<TaraxaPeer> &peer) {
  const auto [current_pbft_round, current_pbft_period] = pbft_mgr_->getPbftRoundAndPeriod();
  const auto current_pbft_step = pbft_mgr_->getPbftStep();
  const auto vote_hash = vote->getHash();

  if (vote->getPeriod() > current_pbft_period + kVoteAcceptingPeriods) {
    // Do not request round sync too often here
    if (std::chrono::system_clock::now() - round_sync_request_time_ > kSyncRequestInterval) {
      // request PBFT chain sync from this node
      sealAndSend(peer->getId(), SubprotocolPacketType::GetPbftSyncPacket,
                  std::move(dev::RLPStream(1) << std::max(vote->getPeriod() - 1, peer->pbft_chain_size_.load())));
      round_sync_request_time_ = std::chrono::system_clock::now();
    }
    LOG(log_dg_) << "Skip vote " << vote_hash.abridged()
                 << ". Failed period validation against current state. Vote: {period: " << vote->getPeriod()
                 << ", round: " << vote->getRound() << ", step: " << vote->getStep()
                 << "}. Current state: {period: " << current_pbft_period << ", round: " << current_pbft_round
                 << ", step: " << pbft_mgr_->getPbftStep() << "}";
    return false;
  }

  auto checking_round = current_pbft_round;
  // If period is not the same we assuming current round is equal to 1
  // So we won't accept votes for future period with round bigger than kVoteAcceptingSteps
  if (current_pbft_period != vote->getPeriod()) {
    checking_round = 1;
  }
  if (vote->getRound() >= checking_round + kVoteAcceptingRounds) {
    LOG(log_dg_) << "Skip vote " << vote_hash.abridged()
                 << ". Failed round validation against current state. Checking round " << checking_round
                 << ". Vote: {period: " << vote->getPeriod() << ", round: " << vote->getRound()
                 << ", step: " << vote->getStep() << "}. Current state: {period: " << current_pbft_period
                 << ", round: " << current_pbft_round << ", step: " << current_pbft_step << "}";
    // Do not request round sync too often here
    if (std::chrono::system_clock::now() - round_sync_request_time_ > kSyncRequestInterval) {
      // request round votes sync from this node
      sealAndSend(peer->getId(), GetVotesSyncPacket,
                  std::move(dev::RLPStream(3) << current_pbft_period << current_pbft_round << 0));
      round_sync_request_time_ = std::chrono::system_clock::now();
    }
    return false;
  }

  auto checking_step = pbft_mgr_->getPbftStep();
  // If round is not the same we assuming current step is equal to 1
  // So we won't accept votes for future rounds with step bigger than kVoteAcceptingSteps
  if (current_pbft_round != vote->getRound()) {
    checking_step = 1;
  }
  if (vote->getStep() >= checking_step + kVoteAcceptingSteps) {
    LOG(log_dg_) << "Skip vote " << vote_hash.abridged()
                 << ". Failed step validation against current state. Checking step " << checking_step
                 << ". Vote: {period: " << vote->getPeriod() << ", round: " << vote->getRound()
                 << ", step: " << vote->getStep() << "}. Current state: {period: " << current_pbft_period
                 << ", round: " << current_pbft_round << ", step: " << current_pbft_step << "}";
    return false;
  }

  return true;
}

void VotePacketHandler::process(const PacketData &packet_data, const std::shared_ptr<TaraxaPeer> &peer) {
  const auto [current_pbft_round, current_pbft_period] = pbft_mgr_->getPbftRoundAndPeriod();

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

    if (vote->getPeriod() == current_pbft_period && (current_pbft_round - 1) == vote->getRound()) {
      if (auto vote_is_valid = validateNextSyncVote(vote); vote_is_valid.first == false) {
        LOG(log_wr_) << "Vote " << vote->getHash()
                     << " from previous round validation failed. Err: " << vote_is_valid.second;
      } else if (!vote_mgr_->insertUniqueVote(vote)) {
        LOG(log_dg_) << "Non unique vote " << vote->getHash() << " (race condition)";
      }
      continue;
    }
    // Standard vote
    if (vote->getPeriod() >= current_pbft_period) {
      if (!vote_mgr_->voteInVerifiedMap(vote)) {
        if (!checkVoteMaxPeriodRoundStep(vote, peer)) {
          continue;
        }

        if (auto vote_is_valid = validateStandardVote(vote); vote_is_valid.first == false) {
          LOG(log_wr_) << "Vote " << vote_hash.abridged() << " validation failed. Err: " << vote_is_valid.second;
          continue;
        }

        if (!vote_mgr_->addVerifiedVote(vote)) {
          LOG(log_dg_) << "Vote " << vote_hash << " already inserted in verified queue(race condition)";
          continue;
        }
      }
    } else if (vote->getPeriod() == current_pbft_period - 1 && vote->getType() == PbftVoteTypes::cert_vote_type) {
      // potential reward vote
      if (!vote_mgr_->isInRewardsVotes(vote->getHash())) {
        if (auto vote_is_valid = validateRewardVote(vote); vote_is_valid.first == false) {
          LOG(log_wr_) << "Reward vote " << vote_hash.abridged() << " validation failed. Err: \""
                       << vote_is_valid.second << "\", vote round " << vote->getRound()
                       << ", current round: " << current_pbft_round << ", vote period: " << vote->getPeriod()
                       << ", current period: " << current_pbft_period
                       << ", vote type: " << static_cast<uint64_t>(vote->getType());
          continue;
        }

        if (!vote_mgr_->addRewardVote(vote)) {
          LOG(log_dg_) << "Reward vote " << vote_hash.abridged() << " already inserted in reward votes(race condition)";
          continue;
        }
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

  onNewPbftVotes(std::move(votes));
}

}  // namespace taraxa::network::tarcap
