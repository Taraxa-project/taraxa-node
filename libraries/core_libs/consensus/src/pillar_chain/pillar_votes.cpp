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

  // Return minimum amount of >threshold sorted votes based on their weight
  if (above_threshold) {
    const auto threshold = found_period_votes->second.threshold;
    if (found_pillar_block_votes->second.weight < threshold) {
      return {};
    }

    // Sort votes using multiset
    auto customComparator = [](const std::pair<std::shared_ptr<PillarVote>, uint64_t>& a,
                               const std::pair<std::shared_ptr<PillarVote>, uint64_t>& b) {
      return a.second > b.second;
    };
    std::multiset<std::pair<std::shared_ptr<PillarVote>, uint64_t>, decltype(customComparator)> votes_set(
        customComparator);
    std::transform(found_pillar_block_votes->second.votes.begin(), found_pillar_block_votes->second.votes.end(),
                   std::inserter(votes_set, votes_set.end()), [](const auto& el) { return el.second; });

    // Move minimum amount of > threshold votes with the highest vote counts
    std::vector<std::shared_ptr<PillarVote>> sorted_votes;
    sorted_votes.reserve(votes_set.size());
    uint64_t tmp_votes_count = 0;
    for (auto it = votes_set.begin(); it != votes_set.end();) {
      auto&& vote_pair = votes_set.extract(it++);
      tmp_votes_count += vote_pair.value().second;
      sorted_votes.push_back(std::move(vote_pair.value().first));

      if (tmp_votes_count >= threshold) {
        break;
      }
    }

    return sorted_votes;
  }

  // Return all votes
  std::vector<std::shared_ptr<PillarVote>> votes;
  votes.reserve(found_pillar_block_votes->second.votes.size());
  for (const auto& sig : found_pillar_block_votes->second.votes) {
    votes.push_back(sig.second.first);
  }

  return votes;
}

bool PillarVotes::periodDataInitialized(PbftPeriod period) const {
  std::shared_lock<std::shared_mutex> lock(mutex_);
  return votes_.contains(period);
}

bool PillarVotes::addVerifiedVote(const std::shared_ptr<PillarVote>& vote, uint64_t validator_vote_count) {
  std::scoped_lock<std::shared_mutex> lock(mutex_);

  auto found_period_votes = votes_.find(vote->getPeriod());
  if (found_period_votes == votes_.end()) {
    // Period must be initialized explicitly providing also threshold weight before adding any vote
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
  if (pillar_block_votes->second.votes.emplace(vote->getHash(), std::make_pair(vote, validator_vote_count)).second) {
    pillar_block_votes->second.weight += validator_vote_count;
  }

  return true;
}

void PillarVotes::initializePeriodData(PbftPeriod period, uint64_t threshold) {
  std::scoped_lock<std::shared_mutex> lock(mutex_);
  votes_.insert({period, PeriodVotes{{}, {}, threshold}});
}

void PillarVotes::eraseVotes(PbftPeriod min_period) {
  std::scoped_lock<std::shared_mutex> lock(mutex_);
  std::erase_if(votes_, [min_period](const auto& item) { return item.first < min_period; });
}

}  // namespace taraxa::pillar_chain