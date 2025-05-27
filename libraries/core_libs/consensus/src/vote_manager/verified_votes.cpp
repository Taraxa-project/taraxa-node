#include "vote_manager/verified_votes.hpp"

#include "common/constants.hpp"
#include "vote/pbft_vote.hpp"

namespace taraxa {

uint64_t VerifiedVotes::size() const {
  uint64_t size = 0;
  std::shared_lock lock(verified_votes_access_);
  for (auto const& [_, period] : verified_votes_) {
    for (auto const& [_, round] : period) {
      for (auto const& [_, step] : round.step_votes) {
        size += std::accumulate(step.votes.begin(), step.votes.end(), 0, [](uint64_t value, const auto& voted_value) {
          return value + voted_value.second.votes.size();
        });
      }
    }
  }
  return size;
}

std::vector<std::shared_ptr<PbftVote>> VerifiedVotes::votes() const {
  std::vector<std::shared_ptr<PbftVote>> votes;
  votes.reserve(size());

  std::shared_lock lock(verified_votes_access_);
  for (const auto& [_, period_votes] : verified_votes_) {
    for (const auto& [_, round_votes] : period_votes) {
      for (const auto& [_, step_votes] : round_votes.step_votes) {
        for (const auto& [_, voted_value] : step_votes.votes) {
          for (const auto& [_, v] : voted_value.votes) {
            votes.emplace_back(v);
          }
        }
      }
    }
  }

  return votes;
}

std::optional<const RoundVerifiedVotesMap> VerifiedVotes::getPeriodVotes(PbftPeriod period) const {
  if (auto [found, it] = get(period); found) {
    return it->second;
  }
  return std::nullopt;
}

std::optional<const RoundVerifiedVotes> VerifiedVotes::getRoundVotes(PbftPeriod period, PbftRound round) const {
  if (auto [found, it] = get(period, round); found) {
    return it->second;
  }
  return std::nullopt;
}

std::optional<const StepVotes> VerifiedVotes::getStepVotes(PbftPeriod period, PbftRound round, PbftStep step) const {
  if (auto [found, it] = get(period, round, step); found) {
    return it->second;
  }
  return std::nullopt;
}

std::optional<VotedBlock> VerifiedVotes::getTwoTPlusOneVotedBlock(PbftPeriod period, PbftRound round,
                                                                  TwoTPlusOneVotedBlockType type) const {
  std::shared_lock lock(verified_votes_access_);

  const auto found_period_it = verified_votes_.find(period);
  if (found_period_it == verified_votes_.end()) {
    return {};
  }

  const auto found_round_it = found_period_it->second.find(round);
  if (found_round_it == found_period_it->second.end()) {
    return {};
  }

  const auto two_t_plus_one_voted_block_it = found_round_it->second.two_t_plus_one_voted_blocks_.find(type);
  if (two_t_plus_one_voted_block_it == found_round_it->second.two_t_plus_one_voted_blocks_.end()) {
    return {};
  }

  return two_t_plus_one_voted_block_it->second;
}

std::vector<std::shared_ptr<PbftVote>> VerifiedVotes::getTwoTPlusOneVotedBlockVotes(
    PbftPeriod period, PbftRound round, TwoTPlusOneVotedBlockType type) const {
  const auto voted = getTwoTPlusOneVotedBlock(period, round, type);
  if (!voted) {
    return {};
  }

  const auto step_votes = getStepVotes(period, round, voted->step);
  if (!step_votes) {
    return {};
  }

  // Find verified votes for specified block_hash based on found 2t+1 voted block of type "type"
  const auto found_verified_votes_it = step_votes->votes.find(voted->hash);
  if (found_verified_votes_it == step_votes->votes.end()) {
    assert(false);
    return {};
  }

  std::vector<std::shared_ptr<PbftVote>> votes;
  votes.reserve(found_verified_votes_it->second.votes.size());
  for (const auto& [_, vote] : found_verified_votes_it->second.votes) {
    votes.push_back(vote);
  }

  return votes;
}

void VerifiedVotes::setNetworkTPlusOneStep(std::shared_ptr<PbftVote> vote) {
  auto [found, round_it] = get(vote->getPeriod(), vote->getRound());
  if (!found) {
    return;
  }

  std::scoped_lock lock(verified_votes_access_);
  round_it->second.network_t_plus_one_step = vote->getStep();
}

void VerifiedVotes::insertTwoTPlusOneVotedBlock(TwoTPlusOneVotedBlockType type, std::shared_ptr<PbftVote> vote) {
  auto [found, round_it] = get(vote->getPeriod(), vote->getRound());
  if (!found) {
    std::cerr << "insertTwoTPlusOneVotedBlock: period " << vote->getPeriod() << ", round " << vote->getRound()
              << std::endl;
    return;
  }

  std::scoped_lock lock(verified_votes_access_);
  round_it->second.two_t_plus_one_voted_blocks_.emplace(type, VotedBlock{vote->getBlockHash(), vote->getStep()});
}

std::optional<std::shared_ptr<PbftVote>> VerifiedVotes::insertUniqueVoter(const std::shared_ptr<PbftVote>& vote) {
  auto [found, step_it] = get(vote->getPeriod(), vote->getRound(), vote->getStep());

  if (!found) {
    return std::nullopt;
  }

  std::scoped_lock lock(verified_votes_access_);
  auto inserted_vote = step_it->second.unique_voters.insert({vote->getVoterAddr(), {vote, nullptr}});

  // Vote was successfully inserted -> it is unique
  if (inserted_vote.second) {
    return std::nullopt;
  }

  const auto existing_vote = inserted_vote.first->second.first;

  // There was already some vote inserted, check if it is the same vote as we are trying to insert
  if (inserted_vote.first->second.first->getHash() != vote->getHash()) {
    // Next votes (second finishing steps) are special case, where we allow voting for both kNullBlockHash and
    // some other specific block hash at the same time -> 2 unique votes per round & step & voter
    if (vote->getType() == PbftVoteTypes::next_vote && vote->getStep() % 2) {
      // New second next vote
      if (inserted_vote.first->second.second == nullptr) {
        // One of the next votes == kNullBlockHash -> valid scenario
        if (inserted_vote.first->second.first->getBlockHash() == kNullBlockHash &&
            vote->getBlockHash() != kNullBlockHash) {
          inserted_vote.first->second.second = vote;
          return std::nullopt;
        } else if (inserted_vote.first->second.first->getBlockHash() != kNullBlockHash &&
                   vote->getBlockHash() == kNullBlockHash) {
          inserted_vote.first->second.second = vote;
          return std::nullopt;
        }
      } else if (inserted_vote.first->second.second->getHash() == vote->getHash()) {
        return std::nullopt;
      }
    }

    std::stringstream err;
    err << "Unable to insert new unique vote(race condition): " << ", new vote hash (voted value): "
        << vote->getHash().abridged() << " (" << vote->getBlockHash() << ")"
        << ", orig. vote hash (voted value): " << inserted_vote.first->second.first->getHash().abridged() << " ("
        << inserted_vote.first->second.first->getBlockHash() << ")";
    if (inserted_vote.first->second.second != nullptr) {
      err << ", orig. vote 2 hash (voted value): " << inserted_vote.first->second.second->getHash().abridged() << " ("
          << inserted_vote.first->second.second->getBlockHash() << ")";
    }
    err << ", period: " << vote->getPeriod() << ", round: " << vote->getRound() << ", step: " << vote->getStep()
        << ", voter: " << vote->getVoterAddr();
    // TODO: proper logging
    // std::cerr << err.str() << std::endl;
    // LOG(log_er_) << err.str();

    // Return existing vote
    if (inserted_vote.first->second.second && vote->getHash() != inserted_vote.first->second.second->getHash()) {
      return inserted_vote.first->second.second;
    }
    return inserted_vote.first->second.first;
  }

  return std::nullopt;
}

std::optional<VotesWithWeight> VerifiedVotes::insertVotedValue(const std::shared_ptr<PbftVote>& vote) {
  std::scoped_lock lock(verified_votes_access_);

  auto found_period_it = verified_votes_.find(vote->getPeriod());
  if (found_period_it == verified_votes_.end()) {
    found_period_it = verified_votes_.insert({vote->getPeriod(), {}}).first;
  }

  auto found_round_it = found_period_it->second.find(vote->getRound());
  if (found_round_it == found_period_it->second.end()) {
    found_round_it = found_period_it->second.insert({vote->getRound(), {}}).first;
  }

  auto found_step_it = found_round_it->second.step_votes.find(vote->getStep());
  if (found_step_it == found_round_it->second.step_votes.end()) {
    found_step_it = found_round_it->second.step_votes.insert({vote->getStep(), {}}).first;
  }

  auto found_voted_value_it = found_step_it->second.votes.find(vote->getBlockHash());
  // Add voted value
  if (found_voted_value_it == found_step_it->second.votes.end()) {
    found_voted_value_it = found_step_it->second.votes.insert({vote->getBlockHash(), {}}).first;
  }

  if (found_voted_value_it->second.votes.contains(vote->getHash())) {
    // TODO: proper logging
    // LOG(log_dg_) << "Vote " << vote->getHash() << " is in verified map already";
    return {};
  }

  // Add vote hash
  if (!found_voted_value_it->second.votes.insert({vote->getHash(), vote}).second) {
    // This should never happen
    // LOG(log_dg_) << "Vote " << vote->getHash() << " is in verified map already (race condition)";
    assert(false);
    return {};
  }
  found_voted_value_it->second.weight += *vote->getWeight();
  return found_voted_value_it->second;
}

void VerifiedVotes::cleanupVotesByPeriod(PbftPeriod pbft_period) {
  // Remove verified votes
  std::scoped_lock lock(verified_votes_access_);
  auto it = verified_votes_.begin();
  while (it != verified_votes_.end() && it->first < pbft_period) {
    it = verified_votes_.erase(it);
  }
}

template <typename IteratorType, typename MapType>
std::pair<bool, IteratorType> getImpl(PbftPeriod period, MapType& map) {
  auto it = map.find(period);
  return {it != map.end(), it};
}

std::pair<bool, PeriodVerifiedVotesMap::iterator> VerifiedVotes::get(PbftPeriod period) {
  std::shared_lock lock(verified_votes_access_);
  return getImpl<PeriodVerifiedVotesMap::iterator>(period, verified_votes_);
}

std::pair<bool, PeriodVerifiedVotesMap::const_iterator> VerifiedVotes::get(PbftPeriod period) const {
  std::shared_lock lock(verified_votes_access_);
  return getImpl<PeriodVerifiedVotesMap::const_iterator>(period, verified_votes_);
}

std::pair<bool, RoundVerifiedVotesMap::iterator> VerifiedVotes::get(PbftPeriod period, PbftRound round) {
  std::shared_lock lock(verified_votes_access_);
  auto res = get(period);
  if (!res.first) {
    return {false, RoundVerifiedVotesMap::iterator{}};
  }
  return getImpl<RoundVerifiedVotesMap::iterator>(round, res.second->second);
}

std::pair<bool, RoundVerifiedVotesMap::const_iterator> VerifiedVotes::get(PbftPeriod period, PbftRound round) const {
  std::shared_lock lock(verified_votes_access_);
  auto res = get(period);
  if (!res.first) {
    return {false, RoundVerifiedVotesMap::const_iterator{}};
  }
  return getImpl<RoundVerifiedVotesMap::const_iterator>(round, res.second->second);
}

std::pair<bool, StepVotesMap::iterator> VerifiedVotes::get(PbftPeriod period, PbftRound round, PbftStep step) {
  std::shared_lock lock(verified_votes_access_);
  auto it = get(period, round);
  if (!it.first) {
    return {false, StepVotesMap::iterator{}};
  }
  auto step_it = it.second->second.step_votes.find(step);
  return {step_it != it.second->second.step_votes.end(), step_it};
}

std::pair<bool, StepVotesMap::const_iterator> VerifiedVotes::get(PbftPeriod period, PbftRound round,
                                                                 PbftStep step) const {
  std::shared_lock lock(verified_votes_access_);
  auto it = get(period, round);
  if (!it.first) {
    return {false, StepVotesMap::const_iterator{}};
  }
  auto step_it = it.second->second.step_votes.find(step);
  return {step_it != it.second->second.step_votes.end(), step_it};
}

}  // namespace taraxa
