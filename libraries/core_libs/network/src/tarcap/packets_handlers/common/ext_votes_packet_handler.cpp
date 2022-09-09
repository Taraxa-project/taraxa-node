#include "network/tarcap/packets_handlers/common/ext_votes_packet_handler.hpp"

#include "pbft/pbft_manager.hpp"
#include "vote/vote.hpp"
#include "vote_manager/vote_manager.hpp"

namespace taraxa::network::tarcap {

ExtVotesPacketHandler::ExtVotesPacketHandler(std::shared_ptr<PeersState> peers_state,
                                             std::shared_ptr<PacketsStats> packets_stats,
                                             std::shared_ptr<PbftManager> pbft_mgr,
                                             std::shared_ptr<PbftChain> pbft_chain,
                                             std::shared_ptr<VoteManager> vote_mgr, const NetworkConfig &net_config,
                                             const addr_t &node_addr, const std::string &log_channel_name)
    : PacketHandler(std::move(peers_state), std::move(packets_stats), node_addr, log_channel_name),
      kVoteAcceptingPeriods(net_config.vote_accepting_periods),
      kVoteAcceptingRounds(net_config.vote_accepting_rounds),
      kVoteAcceptingSteps(net_config.vote_accepting_steps),
      last_votes_sync_request_time_(std::chrono::system_clock::now()),
      last_pbft_block_sync_request_time_(std::chrono::system_clock::now()),
      pbft_mgr_(std::move(pbft_mgr)),
      pbft_chain_(std::move(pbft_chain)),
      vote_mgr_(std::move(vote_mgr)) {}

bool ExtVotesPacketHandler::processStandardVote(const std::shared_ptr<Vote> &vote,
                                                const std::shared_ptr<TaraxaPeer> &peer, bool validate_max_round_step) {
  if (vote_mgr_->voteInVerifiedMap(vote)) {
    LOG(log_dg_) << "Vote " << vote->getHash() << " already inserted in verified queue";
    return false;
  }

  if (auto vote_is_valid = validateStandardVote(vote, peer, validate_max_round_step); !vote_is_valid.first) {
    // There is a possible race condition:
    // 1) vote is evaluated as standard vote during processing vote packet based on current_pbft_period == vote_period
    // 2) In the meantime new block is pushed
    // 3) Standard vote validation then fails due to invalid period
    // -> If this happens, try to process this vote as reward vote
    if (vote->getType() == PbftVoteTypes::cert_vote_type && vote->getPeriod() == pbft_mgr_->getPbftPeriod() - 1) {
      LOG(log_dg_) << "Process standard cert vote as reward vote " << vote->getHash();
      if (processRewardVote(vote)) {
        return true;
      }
    }

    LOG(log_wr_) << "Vote " << vote->getHash() << " validation failed. Err: " << vote_is_valid.second;
    return false;
  }

  if (!vote_mgr_->addVerifiedVote(vote)) {
    LOG(log_dg_) << "Vote " << vote->getHash() << " already inserted in verified queue(race condition)";
    return false;
  }

  return true;
}

bool ExtVotesPacketHandler::processRewardVote(const std::shared_ptr<Vote> &vote) const {
  if (vote_mgr_->isInRewardsVotes(vote->getHash())) {
    LOG(log_dg_) << "Reward vote " << vote->getHash() << " already inserted in reward votes";
    return false;
  }

  if (auto vote_is_valid = validateRewardVote(vote); !vote_is_valid.first) {
    LOG(log_wr_) << "Reward vote " << vote->getHash() << " validation failed. Err: " << vote_is_valid.second;
    return false;
  }

  if (!vote_mgr_->addRewardVote(vote)) {
    LOG(log_dg_) << "Reward vote " << vote->getHash() << " already inserted in reward votes(race condition)";
    return false;
  }

  return true;
}

bool ExtVotesPacketHandler::processNextSyncVote(const std::shared_ptr<Vote> &vote) const {
  // TODO: add check if next vote is already in next votes

  if (auto vote_is_valid = validateNextSyncVote(vote); !vote_is_valid.first) {
    LOG(log_wr_) << "Vote " << vote->getHash()
                 << " from previous round validation failed. Err: " << vote_is_valid.second;
    return false;
  }

  if (!vote_mgr_->insertUniqueVote(vote)) {
    LOG(log_dg_) << "Non unique next vote " << vote->getHash() << " (race condition)";
    return false;
  }

  return true;
}

std::pair<bool, std::string> ExtVotesPacketHandler::validateStandardVote(const std::shared_ptr<Vote> &vote,
                                                                         const std::shared_ptr<TaraxaPeer> &peer,
                                                                         bool validate_max_round_step) {
  const auto [current_pbft_round, current_pbft_period] = pbft_mgr_->getPbftRoundAndPeriod();

  auto genErrMsg = [period = current_pbft_period, round = current_pbft_round,
                    step = pbft_mgr_->getPbftStep()](const std::shared_ptr<Vote> &vote) -> std::string {
    std::stringstream err;
    err << "Vote " << vote->getHash() << " (period, round, step) = (" << vote->getPeriod() << ", " << vote->getRound()
        << ", " << vote->getStep() << "). Current PBFT (period, round, step) = (" << period << ", " << round << ", "
        << step << ")";
    return err.str();
  };

  // Period validation
  if (vote->getPeriod() < current_pbft_period) {
    return {false, "Invalid period(too small): " + genErrMsg(vote)};
  } else if (vote->getPeriod() - 1 > current_pbft_period + kVoteAcceptingPeriods) {
    // vote->getPeriod() - 1 is here because votes are validated against vote_period - 1 in dpos contract
    // Do not request round sync too often here
    if (std::chrono::system_clock::now() - last_pbft_block_sync_request_time_ > kSyncRequestInterval) {
      // request PBFT chain sync from this node
      sealAndSend(peer->getId(), SubprotocolPacketType::GetPbftSyncPacket,
                  std::move(dev::RLPStream(1) << std::max(vote->getPeriod() - 1, peer->pbft_chain_size_.load())));
      last_pbft_block_sync_request_time_ = std::chrono::system_clock::now();
    }

    return {false, "Invalid period(too big): " + genErrMsg(vote)};
  }

  // Round validation
  auto checking_round = current_pbft_round;
  // If period is not the same we assume current round is equal to 1
  // So we won't accept votes for future period with round bigger than kVoteAcceptingSteps
  if (current_pbft_period != vote->getPeriod()) {
    checking_round = 1;
  }

  if (vote->getRound() < checking_round) {
    return {false, "Invalid round(too small): " + genErrMsg(vote)};
  } else if (validate_max_round_step && vote->getRound() >= checking_round + kVoteAcceptingRounds) {
    // Trigger votes(round) syncing only if we are in sync in terms of period
    if (current_pbft_period == vote->getPeriod()) {
      // Do not request round sync too often here
      if (std::chrono::system_clock::now() - last_votes_sync_request_time_ > kSyncRequestInterval) {
        // request round votes sync from this node
        requestPbftNextVotesAtPeriodRound(peer->getId(), current_pbft_period, current_pbft_round, 0);
        last_votes_sync_request_time_ = std::chrono::system_clock::now();
      }
    }

    return {false, "Invalid round(too big): " + genErrMsg(vote)};
  }

  // Step validation
  auto checking_step = pbft_mgr_->getPbftStep();
  // If period or round is not the same we assume current step is equal to 1
  // So we won't accept votes for future rounds with step bigger than kVoteAcceptingSteps
  if (current_pbft_period != vote->getPeriod() || current_pbft_round != vote->getRound()) {
    checking_step = 1;
  }

  if (validate_max_round_step && vote->getStep() >= checking_step + kVoteAcceptingSteps) {
    return {false, "Invalid step(too big): " + genErrMsg(vote)};
  }

  return validateVote(vote);
}

std::pair<bool, std::string> ExtVotesPacketHandler::validateNextSyncVote(const std::shared_ptr<Vote> &vote) const {
  const auto [current_pbft_round, current_pbft_period] = pbft_mgr_->getPbftRoundAndPeriod();

  if (vote->getType() != PbftVoteTypes::next_vote_type) {
    std::stringstream err;
    err << "Invalid type: " << static_cast<uint64_t>(vote->getType());
    return {false, err.str()};
  }

  if (vote->getStep() < PbftStates::finish_state) {
    std::stringstream err;
    err << "Invalid step: " << static_cast<uint64_t>(vote->getStep());
    return {false, err.str()};
  }

  // Old vote or vote from too far in the future, can be dropped
  if (vote->getPeriod() != current_pbft_period) {
    std::stringstream err;
    err << "Invalid period: Vote period: " << vote->getPeriod() << ", current pbft period: " << current_pbft_period;
    return {false, err.str()};
  }

  if (vote->getRound() != current_pbft_round - 1) {
    std::stringstream err;
    err << "Invalid round: Vote round: " << vote->getRound() << ", current pbft round: " << current_pbft_round;
    return {false, err.str()};
  }

  // TODO: check also max step

  return validateVote(vote);
}

std::pair<bool, std::string> ExtVotesPacketHandler::validateRewardVote(const std::shared_ptr<Vote> &vote) const {
  const auto [current_pbft_round, current_pbft_period] = pbft_mgr_->getPbftRoundAndPeriod();

  if (vote->getType() != PbftVoteTypes::cert_vote_type) {
    std::stringstream err;
    err << "Invalid type: " << static_cast<uint64_t>(vote->getType());
    return {false, err.str()};
  }

  if (vote->getStep() != PbftStates::certify_state) {
    std::stringstream err;
    err << "Invalid step: " << static_cast<uint64_t>(vote->getStep());
    return {false, err.str()};
  }

  if (vote->getPeriod() != current_pbft_period - 1) {
    std::stringstream err;
    err << "Invalid period: Vote period: " << vote->getPeriod() << ", current pbft period: " << current_pbft_period;
    return {false, err.str()};
  }

  // TODO[1938]: implement ddos protection so user cannot sent indefinite number of valid reward votes with different
  // rounds.

  return validateVote(vote);
}

std::pair<bool, std::string> ExtVotesPacketHandler::validateVote(const std::shared_ptr<Vote> &vote) const {
  // Check is vote is unique per period, round & step & voter -> each address can generate just 1 vote
  // (for a value that isn't NBH) per period, round & step
  if (auto unique_vote_validation = vote_mgr_->isUniqueVote(vote); !unique_vote_validation.first) {
    return unique_vote_validation;
  }

  const auto vote_valid = pbft_mgr_->validateVote(vote);
  if (!vote_valid.first) {
    LOG(log_er_) << "Vote \"dpos\" validation failed: " << vote_valid.second;
  }

  return vote_valid;
}

void ExtVotesPacketHandler::onNewPbftVotes(std::vector<std::shared_ptr<Vote>> &&votes) {
  const auto rewards_votes_block = vote_mgr_->getCurrentRewardsVotesBlock();

  for (const auto &peer : peers_state_->getAllPeers()) {
    if (peer.second->syncing_) {
      continue;
    }

    std::vector<std::shared_ptr<Vote>> send_votes;
    for (const auto &v : votes) {
      if (!peer.second->isVoteKnown(v->getHash()) &&
          // CONCERN ... should we be using a period stored in peer state?
          (peer.second->pbft_chain_size_ <= v->getPeriod() - 1 || peer.second->pbft_round_ <= v->getRound() ||
           (v->getType() == cert_vote_type && v->getBlockHash() == rewards_votes_block.first /* reward vote */))) {
        send_votes.push_back(v);
      }
    }
    sendPbftVotes(peer.first, std::move(send_votes));
  }
}

void ExtVotesPacketHandler::sendPbftVotes(const dev::p2p::NodeID &peer_id, std::vector<std::shared_ptr<Vote>> &&votes,
                                          bool is_next_votes) {
  if (votes.empty()) {
    return;
  }

  LOG(log_nf_) << "Will send next votes type " << std::boolalpha << is_next_votes;
  auto subprotocol_packet_type =
      is_next_votes ? SubprotocolPacketType::VotesSyncPacket : SubprotocolPacketType::VotePacket;

  size_t index = 0;
  while (index < votes.size()) {
    const size_t count = std::min(static_cast<size_t>(kMaxVotesInPacket), votes.size() - index);
    dev::RLPStream s(count);
    for (auto i = index; i < index + count; i++) {
      const auto &v = votes[i];
      // Withou sending also vote weight,
      // check_committeeSize_less_or_equal_to_activePlayers & check_committeeSize_greater_than_activePlayers tests fail
      s.appendRaw(v->rlp(true, false));
      LOG(log_dg_) << "Send out vote " << v->getHash() << " to peer " << peer_id;
    }

    if (sealAndSend(peer_id, subprotocol_packet_type, std::move(s))) {
      LOG(log_nf_) << "Send out size of " << count << " PBFT votes to " << peer_id;
    }

    index += count;
  }

  if (const auto peer = peers_state_->getPeer(peer_id)) {
    for (const auto &v : votes) {
      peer->markVoteAsKnown(v->getHash());
    }
  }
}

}  // namespace taraxa::network::tarcap
