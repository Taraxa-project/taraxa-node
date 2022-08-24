#include "network/tarcap/packets_handlers/common/ext_votes_packet_handler.hpp"

#include "pbft/pbft_manager.hpp"
#include "vote/vote.hpp"
#include "vote_manager/vote_manager.hpp"

namespace taraxa::network::tarcap {

ExtVotesPacketHandler::ExtVotesPacketHandler(std::shared_ptr<PeersState> peers_state,
                                             std::shared_ptr<PacketsStats> packets_stats,
                                             std::shared_ptr<PbftManager> pbft_mgr,
                                             std::shared_ptr<PbftChain> pbft_chain,
                                             std::shared_ptr<VoteManager> vote_mgr, const uint32_t dpos_delay,
                                             const addr_t &node_addr, const std::string &log_channel_name)
    : PacketHandler(std::move(peers_state), std::move(packets_stats), node_addr, log_channel_name),
      kDposDelay(dpos_delay),
      pbft_mgr_(std::move(pbft_mgr)),
      pbft_chain_(std::move(pbft_chain)),
      vote_mgr_(std::move(vote_mgr)) {}

std::pair<bool, std::string> ExtVotesPacketHandler::validateStandardVote(const std::shared_ptr<Vote> &vote) const {
  const auto [current_pbft_round, current_pbft_period] = pbft_mgr_->getPbftRoundAndPeriod();

  // Old vote or vote from too far in the future, can be dropped
  // TODO[1880]: should be vote->getPeriod() <= current_pbft_period - if <=, some tests are failing due to missing
  // reward votes -> whole rewards votes gossiping need to be checked...

  // CONCERN: Why the minus one on the vote period?
  if (vote->getPeriod() < current_pbft_period || vote->getPeriod() - 1 > current_pbft_period + kDposDelay) {
    std::stringstream err;
    err << "Invalid period: Vote period: " << vote->getPeriod() << ", current pbft period: " << current_pbft_period;
    return {false, err.str()};
  }

  if (vote->getRound() < current_pbft_round) {
    std::stringstream err;
    err << "Invalid round: Vote round: " << vote->getRound() << ", current pbft round: " << current_pbft_round;
    return {false, err.str()};
  }

  if (const auto [voter_max_period, voter_max_round] = getVoterMaxPeriodAndRound(vote->getVoterAddr());
      vote->getPeriod() < voter_max_period || vote->getRound() + 1 < voter_max_round) {
    std::stringstream err;
    err << "Invalid voute period, round: Vote period, round: {" << vote->getPeriod() << ", " << vote->getRound()
        << "}, voter current max received period, round: {" << voter_max_period << ", " << voter_max_round << "}";
    return {false, err.str()};
  }

  return validateVote(vote);
}

std::pair<bool, std::string> ExtVotesPacketHandler::validateNextSyncVote(const std::shared_ptr<Vote> &vote) const {
  const auto [current_pbft_round, current_pbft_period] = pbft_mgr_->getPbftRoundAndPeriod();

  // Old vote or vote from too far in the future, can be dropped
  // CONCERN: Why the minus one on the vote period?
  if (vote->getPeriod() < current_pbft_period || vote->getPeriod() - 1 > current_pbft_period + kDposDelay) {
    std::stringstream err;
    err << "Invalid period: Vote period: " << vote->getPeriod() << ", current pbft period: " << current_pbft_period;
    return {false, err.str()};
  }

  if (vote->getRound() < current_pbft_round - 1) {
    std::stringstream err;
    err << "Invalid round: Vote round: " << vote->getRound() << ", current pbft round: " << current_pbft_round;
    return {false, err.str()};
  }

  if (vote->getType() != next_vote_type) {
    std::stringstream err;
    err << "Invalid type: " << static_cast<uint64_t>(vote->getType());
    return {false, err.str()};
  }

  return validateVote(vote);
}

std::pair<bool, std::string> ExtVotesPacketHandler::validateRewardVote(const std::shared_ptr<Vote> &vote) const {
  const auto [current_pbft_round, current_pbft_period] = pbft_mgr_->getPbftRoundAndPeriod();

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

void ExtVotesPacketHandler::setVoterMaxPeriodAndRound(const addr_t &voter, uint64_t period, uint64_t round) {
  if (const auto [voter_max_period, voter_max_round] = getVoterMaxPeriodAndRound(voter);
      period < voter_max_period || (period == voter_max_period && voter_max_round >= round)) {
    return;
  }

  std::unique_lock lock(voters_max_periodround_mutex_);
  if (auto inserted_value = voters_max_periodound_.insert({voter, {period, round}}); !inserted_value.second) {
    if (period > inserted_value.first->second.first ||
        (period == inserted_value.first->second.first && round > inserted_value.first->second.second)) {
      inserted_value.first->second = {period, round};
    }
  }
}

std::pair<uint64_t, uint64_t> ExtVotesPacketHandler::getVoterMaxPeriodAndRound(const addr_t &voter) const {
  std::shared_lock lock(voters_max_periodround_mutex_);

  auto voter_max_periodround = voters_max_periodound_.find(voter);
  return voter_max_periodround == voters_max_periodound_.end() ? std::make_pair<uint64_t, uint64_t>(1, 0)
                                                               : voter_max_periodround->second;
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
