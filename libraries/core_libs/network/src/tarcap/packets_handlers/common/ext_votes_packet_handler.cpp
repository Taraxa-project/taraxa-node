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
                                                const std::shared_ptr<PbftBlock> &pbft_block,
                                                const std::shared_ptr<TaraxaPeer> &peer, bool validate_max_round_step) {
  if (pbft_block && !validateVoteAndBlock(vote, pbft_block)) {
    throw MaliciousPeerException("Received vote's voted value != received pbft block");
  }

  if (vote_mgr_->voteInVerifiedMap(vote)) {
    LOG(log_dg_) << "Vote " << vote->getHash() << " already inserted in verified queue";
    return false;
  }

  if (auto vote_is_valid = validateStandardVote(vote, peer, validate_max_round_step); !vote_is_valid.first) {
    const auto [current_pbft_round, current_pbft_period] = pbft_mgr_->getPbftRoundAndPeriod();
    // There is a possible race condition:
    // 1) vote is evaluated as standard vote during vote packet processing based on current_pbft_period == vote_period
    // 2) In the meantime new block is pushed
    // 3) Standard vote validation then fails due to invalid period
    // -> If this happens, try to process this vote as reward vote
    if (vote->getType() == PbftVoteTypes::cert_vote && vote->getPeriod() == current_pbft_period - 1) {
      LOG(log_dg_) << "Process standard cert vote as reward vote " << vote->getHash();
      if (processRewardVote(vote)) {
        return true;
      }
    } else if (vote->getType() == PbftVoteTypes::next_vote && vote->getPeriod() == current_pbft_period &&
               vote->getRound() == current_pbft_round - 1) {
      if (processNextSyncVote(vote, nullptr)) {
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

  if (pbft_block) {
    pbft_mgr_->processProposedBlock(pbft_block, vote);
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

bool ExtVotesPacketHandler::processNextSyncVote(const std::shared_ptr<Vote> &vote,
                                                const std::shared_ptr<PbftBlock> &pbft_block) const {
  // TODO: add check if next vote is already in next votes

  if (pbft_block && !validateVoteAndBlock(vote, pbft_block)) {
    throw MaliciousPeerException("Received vote's voted value != received pbft block");
  }

  if (auto vote_is_valid = validateNextSyncVote(vote); !vote_is_valid.first) {
    LOG(log_wr_) << "Vote " << vote->getHash()
                 << " from previous round validation failed. Err: " << vote_is_valid.second;
    return false;
  }

  if (!vote_mgr_->insertUniqueVote(vote)) {
    LOG(log_dg_) << "Non unique next vote " << vote->getHash() << " (race condition)";
    return false;
  }

  if (pbft_block) {
    pbft_mgr_->processProposedBlock(pbft_block, vote);
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
  } else if (kVoteAcceptingPeriods && vote->getPeriod() - 1 > current_pbft_period + kVoteAcceptingPeriods) {
    // skip this check if kVoteAcceptingPeriods == 0
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
  } else if (validate_max_round_step && kVoteAcceptingRounds &&
             vote->getRound() >= checking_round + kVoteAcceptingRounds) {
    // skip this check if kVoteAcceptingRounds == 0
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

  // skip check if kVoteAcceptingSteps == 0
  if (validate_max_round_step && kVoteAcceptingSteps && vote->getStep() >= checking_step + kVoteAcceptingSteps) {
    return {false, "Invalid step(too big): " + genErrMsg(vote)};
  }

  return validateVote(vote);
}

std::pair<bool, std::string> ExtVotesPacketHandler::validateNextSyncVote(const std::shared_ptr<Vote> &vote) const {
  const auto [current_pbft_round, current_pbft_period] = pbft_mgr_->getPbftRoundAndPeriod();

  if (vote->getType() != PbftVoteTypes::next_vote) {
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

  if (vote->getType() != PbftVoteTypes::cert_vote) {
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

bool ExtVotesPacketHandler::validateVoteAndBlock(const std::shared_ptr<Vote> &vote,
                                                 const std::shared_ptr<PbftBlock> &pbft_block) const {
  if (pbft_block->getBlockHash() != vote->getBlockHash()) {
    LOG(log_er_) << "Vote " << vote->getHash() << " voted block " << vote->getBlockHash() << " != actual block "
                 << pbft_block->getBlockHash();
    return false;
  }

  return true;
}

void ExtVotesPacketHandler::onNewPbftVote(const std::shared_ptr<Vote> &vote, const std::shared_ptr<PbftBlock> &block) {
  for (const auto &peer : peers_state_->getAllPeers()) {
    if (peer.second->syncing_) {
      LOG(log_dg_) << " PBFT vote " << vote->getHash() << " not sent to " << peer.first << " peer syncing";
      continue;
    }

    if (peer.second->isVoteKnown(vote->getHash())) {
      continue;
    }

    // Peer already has pbft block, do not send it (do not check it for propose votes as it could happen that nodes
    // re-propose the same block for new round, in which case we need to send the block again
    if (vote->getType() != PbftVoteTypes::propose_vote && peer.second->isPbftBlockKnown(vote->getBlockHash())) {
      sendPbftVote(peer.second, vote, nullptr);
    } else {
      sendPbftVote(peer.second, vote, block);
    }
  }
}

void ExtVotesPacketHandler::sendPbftVote(const std::shared_ptr<TaraxaPeer> &peer, const std::shared_ptr<Vote> &vote,
                                         const std::shared_ptr<PbftBlock> &block) {
  if (block && block->getBlockHash() != vote->getBlockHash()) {
    LOG(log_er_) << "Vote " << vote->getHash().abridged() << " voted block " << vote->getBlockHash().abridged()
                 << " != actual block " << block->getBlockHash().abridged();
    return;
  }

  dev::RLPStream s(1);
  if (block) {
    s.appendList(kVotePacketWithBlockSize);
    s.appendRaw(vote->rlp(true, false));
    s.appendRaw(block->rlp(true));
    s.append(pbft_chain_->getPbftChainSize());
  } else {
    s.appendRaw(vote->rlp(true, false));
  }

  if (sealAndSend(peer->getId(), SubprotocolPacketType::VotePacket, std::move(s))) {
    peer->markVoteAsKnown(vote->getHash());
    if (block) {
      peer->markPbftBlockAsKnown(block->getBlockHash());
      LOG(log_dg_) << " PBFT vote " << vote->getHash() << " together with block " << block->getBlockHash()
                   << " sent to " << peer->getId();
    } else {
      LOG(log_dg_) << " PBFT vote " << vote->getHash() << " sent to " << peer->getId();
    }
  }
}

void ExtVotesPacketHandler::onNewPbftVotes(std::vector<std::shared_ptr<Vote>> &&votes, bool rebroadcast) {
  for (const auto &peer : peers_state_->getAllPeers()) {
    if (peer.second->syncing_) {
      continue;
    }

    std::vector<std::shared_ptr<Vote>> peer_votes;
    for (const auto &vote : votes) {
      if (!rebroadcast && peer.second->isVoteKnown(vote->getHash())) {
        continue;
      }

      peer_votes.push_back(vote);
    }

    sendPbftVotes(peer.second, std::move(peer_votes));
  }
}

void ExtVotesPacketHandler::sendPbftVotes(const std::shared_ptr<TaraxaPeer> &peer,
                                          std::vector<std::shared_ptr<Vote>> &&votes, bool is_next_votes) {
  if (votes.empty()) {
    return;
  }

  LOG(log_nf_) << "Send next votes type " << std::boolalpha << is_next_votes;
  auto subprotocol_packet_type =
      is_next_votes ? SubprotocolPacketType::VotesSyncPacket : SubprotocolPacketType::VotePacket;

  size_t index = 0;
  while (index < votes.size()) {
    const size_t count = std::min(static_cast<size_t>(kMaxVotesInPacket), votes.size() - index);
    dev::RLPStream s(count);
    for (auto i = index; i < index + count; i++) {
      const auto &vote = votes[i];
      s.appendRaw(vote->rlp(true, false));
      LOG(log_dg_) << "Send vote " << vote->getHash() << " to peer " << peer->getId();
    }

    if (sealAndSend(peer->getId(), subprotocol_packet_type, std::move(s))) {
      LOG(log_dg_) << count << " PBFT votes to were sent to " << peer->getId();
      for (auto i = index; i < index + count; i++) {
        peer->markVoteAsKnown(votes[i]->getHash());
      }
    }

    index += count;
  }
}

}  // namespace taraxa::network::tarcap
