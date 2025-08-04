#include "network/tarcap/packets_handlers/latest/common/ext_votes_packet_handler.hpp"

#include "network/tarcap/packets/latest/get_next_votes_bundle_packet.hpp"
#include "network/tarcap/packets_handlers/latest/common/exceptions.hpp"
#include "pbft/pbft_manager.hpp"
#include "vote/pbft_vote.hpp"
#include "vote/votes_bundle_rlp.hpp"
#include "vote_manager/vote_manager.hpp"

namespace taraxa::network::tarcap {

ExtVotesPacketHandler::ExtVotesPacketHandler(const FullNodeConfig &conf, std::shared_ptr<PeersState> peers_state,
                                             std::shared_ptr<TimePeriodPacketsStats> packets_stats,
                                             std::shared_ptr<PbftManager> pbft_mgr,
                                             std::shared_ptr<PbftChain> pbft_chain,
                                             std::shared_ptr<VoteManager> vote_mgr,
                                             std::shared_ptr<SlashingManager> slashing_manager, const addr_t &node_addr,
                                             const std::string &log_channel_name)
    : PacketHandler(conf, std::move(peers_state), std::move(packets_stats), node_addr, log_channel_name),
      last_votes_sync_request_time_(std::chrono::system_clock::now()),
      last_pbft_block_sync_request_time_(std::chrono::system_clock::now()),
      pbft_mgr_(std::move(pbft_mgr)),
      pbft_chain_(std::move(pbft_chain)),
      vote_mgr_(std::move(vote_mgr)),
      slashing_manager_(std::move(slashing_manager)) {}

bool ExtVotesPacketHandler::processVote(const std::shared_ptr<PbftVote> &vote,
                                        const std::shared_ptr<PbftBlock> &pbft_block,
                                        const std::shared_ptr<TaraxaPeer> &peer, bool validate_max_round_step) {
  if (pbft_block && !validateVoteAndBlock(vote, pbft_block)) {
    throw MaliciousPeerException("Received vote's voted value != received pbft block");
  }

  if (vote_mgr_->voteInVerifiedMap(vote)) {
    LOG(log_dg_) << "Vote " << vote->getHash() << " already inserted in verified queue";
    return false;
  }

  // Validate vote's period, round and step min/max values
  if (const auto vote_valid = validateVotePeriodRoundStep(vote, peer, validate_max_round_step); !vote_valid.first) {
    LOG(log_wr_) << "Vote period/round/step " << vote->getHash() << " validation failed. Err: " << vote_valid.second;
    return false;
  }

  // Check if vote is unique per period, round & step & voter -> each address can generate just 1 vote
  // (for a value that isn't NBH) per period, round & step
  if (auto vote_valid = vote_mgr_->isUniqueVote(vote); !vote_valid.first) {
    // Create double voting proof
    slashing_manager_->submitDoubleVotingProof(vote, vote_valid.second);
    throw MaliciousPeerException("Received double vote", vote->getVoter());
  }

  // Validate vote's signature, vrf, etc...
  if (const auto vote_valid = vote_mgr_->validateVote(vote); !vote_valid.first) {
    LOG(log_wr_) << "Vote " << vote->getHash() << " validation failed. Err: " << vote_valid.second;
    return false;
  }

  if (!vote_mgr_->addVerifiedVote(vote)) {
    LOG(log_dg_) << "Vote " << vote->getHash() << " already inserted in verified queue(race condition)";
    return false;
  }

  if (pbft_block) {
    pbft_mgr_->processProposedBlock(pbft_block);
  }

  return true;
}

std::pair<bool, std::string> ExtVotesPacketHandler::validateVotePeriodRoundStep(const std::shared_ptr<PbftVote> &vote,
                                                                                const std::shared_ptr<TaraxaPeer> &peer,
                                                                                bool validate_max_round_step) {
  const auto [current_pbft_round, current_pbft_period] = pbft_mgr_->getPbftRoundAndPeriod();

  auto genErrMsg = [period = current_pbft_period, round = current_pbft_round,
                    step = pbft_mgr_->getPbftStep()](const std::shared_ptr<PbftVote> &vote) -> std::string {
    std::stringstream err;
    err << "Vote " << vote->getHash() << " (period, round, step) = (" << vote->getPeriod() << ", " << vote->getRound()
        << ", " << vote->getStep() << "). Current PBFT (period, round, step) = (" << period << ", " << round << ", "
        << step << ")";
    return err.str();
  };

  // Period validation
  auto reward_votes_period_offset = vote_mgr_->getRewardVotesPeriodOffset(vote->getPeriod());
  if ((vote->getType() != PbftVoteTypes::cert_vote && vote->getPeriod() < current_pbft_period) ||
      (vote->getType() == PbftVoteTypes::cert_vote &&
       vote->getPeriod() + reward_votes_period_offset < current_pbft_period /* potential reward vote */)) {
    return {false, "Invalid period(too small): " + genErrMsg(vote)};
  } else if (this->kConf.network.ddos_protection.vote_accepting_periods &&
             vote->getPeriod() - 1 > current_pbft_period + this->kConf.network.ddos_protection.vote_accepting_periods) {
    // skip this check if kConf.network.ddos_protection.vote_accepting_periods == 0
    // vote->getPeriod() - 1 is here because votes are validated against vote_period - 1 in dpos contract
    // Do not request round sync too often here
    if (vote->getVoter() == peer->getId() &&
        std::chrono::system_clock::now() - last_pbft_block_sync_request_time_ > kSyncRequestInterval) {
      // request PBFT chain sync from this node
      this->sealAndSend(
          peer->getId(), SubprotocolPacketType::kGetPbftSyncPacket,
          encodePacketRlp(GetPbftSyncPacket{std::max(vote->getPeriod() - 1, peer->pbft_chain_size_.load())}));
      last_pbft_block_sync_request_time_ = std::chrono::system_clock::now();
    }

    return {false, "Invalid period(too big): " + genErrMsg(vote)};
  }

  // Round validation
  auto checking_round = current_pbft_round;
  // If period is not the same we assume current round is equal to 1
  // So we won't accept votes for future period with round bigger than kConf.network.vote_accepting_steps
  if (current_pbft_period != vote->getPeriod()) {
    checking_round = 1;
  }

  // vote->getRound() == checking_round - 1 && next_vote -> previous round next vote
  if (vote->getRound() < checking_round - 1 ||
      (vote->getRound() == checking_round - 1 && vote->getType() != PbftVoteTypes::next_vote)) {
    return {false, "Invalid round(too small): " + genErrMsg(vote)};
  } else if (validate_max_round_step && this->kConf.network.ddos_protection.vote_accepting_rounds &&
             vote->getRound() >= checking_round + this->kConf.network.ddos_protection.vote_accepting_rounds) {
    // skip this check if kConf.network.vote_accepting_rounds == 0
    // Trigger votes(round) syncing only if we are in sync in terms of period
    if (current_pbft_period == vote->getPeriod()) {
      // Do not request round sync too often here
      if (vote->getVoter() == peer->getId() &&
          std::chrono::system_clock::now() - last_votes_sync_request_time_ > kSyncRequestInterval) {
        // request round votes sync from this node
        this->requestPbftNextVotesAtPeriodRound(peer->getId(), current_pbft_period, current_pbft_round);
        last_votes_sync_request_time_ = std::chrono::system_clock::now();
      }
    }

    return {false, "Invalid round(too big): " + genErrMsg(vote)};
  }

  // Step validation
  auto checking_step = pbft_mgr_->getPbftStep();
  // If period or round is not the same we assume current step is equal to 1
  // So we won't accept votes for future rounds with step bigger than kConf.network.vote_accepting_steps
  if (current_pbft_period != vote->getPeriod() || current_pbft_round != vote->getRound()) {
    checking_step = 1;
  }

  // skip check if kConf.network.vote_accepting_steps == 0
  if (validate_max_round_step && this->kConf.network.ddos_protection.vote_accepting_steps &&
      vote->getStep() >= checking_step + this->kConf.network.ddos_protection.vote_accepting_steps) {
    return {false, "Invalid step(too big): " + genErrMsg(vote)};
  }

  return {true, ""};
}

bool ExtVotesPacketHandler::validateVoteAndBlock(const std::shared_ptr<PbftVote> &vote,
                                                 const std::shared_ptr<PbftBlock> &pbft_block) const {
  if (pbft_block->getPeriod() != vote->getPeriod()) {
    LOG(this->log_er_) << "Vote " << vote->getHash() << " period " << vote->getPeriod() << " != pbft block block "
                       << pbft_block->getPeriod();
    return false;
  }

  if (pbft_block->getBlockHash() != vote->getBlockHash()) {
    LOG(this->log_er_) << "Vote " << vote->getHash() << " voted block " << vote->getBlockHash() << " != actual block "
                       << pbft_block->getBlockHash();
    return false;
  }

  return true;
}

bool ExtVotesPacketHandler::isPbftRelevantVote(const std::shared_ptr<PbftVote> &vote) const {
  const auto [current_pbft_round, current_pbft_period] = pbft_mgr_->getPbftRoundAndPeriod();

  if (vote->getPeriod() >= current_pbft_period && vote->getRound() >= current_pbft_round) {
    // Standard current or future vote
    return true;
  } else if (vote->getPeriod() == current_pbft_period && vote->getRound() == (current_pbft_round - 1) &&
             vote->getType() == PbftVoteTypes::next_vote) {
    // Previous round next vote
    return true;
  } else if (vote->getType() == PbftVoteTypes::cert_vote &&
             vote->getPeriod() + vote_mgr_->getRewardVotesPeriodOffset(current_pbft_period) >= current_pbft_period &&
             vote->getPeriod() < current_pbft_period) {
    // Potential reward vote
    return true;
  }

  return false;
}

void ExtVotesPacketHandler::requestPbftNextVotesAtPeriodRound(const dev::p2p::NodeID &peerID, PbftPeriod pbft_period,
                                                              PbftRound pbft_round) {
  LOG(log_dg_) << "Sending GetNextVotesSyncPacket with period:" << pbft_period << ", round:" << pbft_round;
  const auto packet = GetNextVotesBundlePacket{.peer_pbft_period = pbft_period, .peer_pbft_round = pbft_round};
  sealAndSend(peerID, SubprotocolPacketType::kGetNextVotesSyncPacket, encodePacketRlp(packet));
}

}  // namespace taraxa::network::tarcap
