#include "pillar_chain/pillar_votes.hpp"

namespace taraxa::pillar_chain {

bool PillarVotes::voteExists(const std::shared_ptr<PillarVote> vote) const {
  std::shared_lock<std::shared_mutex> lock(mutex_);

  const auto found_period_votes = votes_.find(vote->getPeriod());
  if (found_period_votes == votes_.end()) {
    return false;
  }

  const auto found_pillar_block_votes = found_period_votes->second.pillar_block_votes.find(vote->getBlockHash());
  if (found_pillar_block_votes == found_period_votes->second.pillar_block_votes.end()) {
    return false;
  }

  return found_pillar_block_votes->second.votes.contains(vote->getHash());
}

bool PillarVotes::isUniqueVote(const std::shared_ptr<PillarVote> vote) const {
  std::shared_lock<std::shared_mutex> lock(mutex_);

  const auto found_period_votes = votes_.find(vote->getPeriod());
  if (found_period_votes == votes_.end()) {
    return true;
  }

  const auto found_validator = found_period_votes->second.unique_voters.find(vote->getVoterAddr());
  if (found_validator == found_period_votes->second.unique_voters.end()) {
    return true;
  }

  if (found_validator->second == vote->getHash()) {
    return true;
  }

  return false;
}

bool PillarVotes::hasAboveThresholdVotes(PbftPeriod period, const blk_hash_t& block_hash) const {
  std::shared_lock<std::shared_mutex> lock(mutex_);

  const auto found_period_votes = votes_.find(period);
  if (found_period_votes == votes_.end()) [[unlikely]] {
    return false;
  }

  const auto found_pillar_block_votes = found_period_votes->second.pillar_block_votes.find(block_hash);
  if (found_pillar_block_votes == found_period_votes->second.pillar_block_votes.end()) [[unlikely]] {
    return false;
  }

  if (found_pillar_block_votes->second.weight < found_period_votes->second.threshold) {
    return false;
  }

  // There is > threshold votes
  return true;
}

std::vector<std::shared_ptr<PillarVote>> PillarVotes::getVerifiedVotes(PbftPeriod period,
                                                                       const blk_hash_t& pillar_block_hash,
                                                                       bool above_threshold) const {
  std::shared_lock<std::shared_mutex> lock(mutex_);
  const auto found_period_votes = votes_.find(period);
  if (found_period_votes == votes_.end()) {
    return {};
  }

  const auto found_pillar_block_votes = found_period_votes->second.pillar_block_votes.find(pillar_block_hash);
  if (found_pillar_block_votes == found_period_votes->second.pillar_block_votes.end()) {
    return {};
  }

  if (above_threshold && found_pillar_block_votes->second.weight < found_period_votes->second.threshold) {
    return {};
  }

  std::vector<std::shared_ptr<PillarVote>> votes;
  votes.reserve(found_pillar_block_votes->second.votes.size());
  for (const auto& sig : found_pillar_block_votes->second.votes) {
    votes.push_back(sig.second);
  }

  return votes;
}

bool PillarVotes::periodDataInitialized(PbftPeriod period) const {
  std::shared_lock<std::shared_mutex> lock(mutex_);
  return votes_.contains(period);
}

bool PillarVotes::addVerifiedVote(const std::shared_ptr<PillarVote>& vote, u_int64_t validator_vote_count) {
  std::scoped_lock<std::shared_mutex> lock(mutex_);

  auto found_period_votes = votes_.find(vote->getPeriod());
  if (found_period_votes == votes_.end()) {
    // Period must be initialized explicitly providing also pillar consensus threshold before adding any vote
    assert(false);
    return false;
  }

  if (const auto unique_validator =
          found_period_votes->second.unique_voters.emplace(vote->getVoterAddr(), vote->getHash());
      !unique_validator.second) {
    if (unique_validator.first->second != vote->getHash()) {
      // Non unique vote for the validator
      return false;
    }
  }

  auto pillar_block_votes = found_period_votes->second.pillar_block_votes.insert({vote->getBlockHash(), {}}).first;

  // Add validator vote count only if the vote is new
  if (pillar_block_votes->second.votes.emplace(vote->getHash(), vote).second) {
    pillar_block_votes->second.weight += validator_vote_count;
  }

  return true;
}

void PillarVotes::initializePeriodData(PbftPeriod period, uint64_t threshold) {
  std::scoped_lock<std::shared_mutex> lock(mutex_);
  votes_.insert({period, PeriodVotes{.threshold = threshold}});
}

void PillarVotes::eraseVotes(PbftPeriod min_period) {
  std::scoped_lock<std::shared_mutex> lock(mutex_);
  std::erase_if(votes_, [min_period](const auto& item) { return item.first < min_period; });
}

}  // namespace taraxa::pillar_chain