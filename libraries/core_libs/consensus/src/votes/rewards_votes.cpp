#include "votes/rewards_votes.hpp"

#include <mutex>

#include "vote/vote.hpp"

namespace taraxa {

void RewardsVotes::resetVotes(std::unordered_set<vote_hash_t>&& cert_votes, const blk_hash_t& voted_block_hash) {
  // previous_period_cert_votes_ = cert_votes_ + new_votes_
  // Set previous_period_cert_votes_ before overriding other containers due to possible race conditions with
  // checkBlockRewardsVotes function, which checks containers in order:
  // 1. cert_votes, 2. new_votes, 3. previous_period_cert_votes_
  {
    std::scoped_lock lock(previous_period_cert_votes_mutex_);
    previous_period_cert_votes_.clear();

    // Copies cert_votes_ into the previous_period_cert_votes_
    {
      std::shared_lock shared_lock(cert_votes_mutex_);
      previous_period_cert_votes_ = cert_votes_;
    }

    // Copies new_votes_ into the previous_period_cert_votes_
    {
      std::shared_lock shared_lock(new_votes_mutex_);
      for (const auto& new_vote : new_votes_) {
        previous_period_cert_votes_.insert(new_vote.first);
      }
    }
  }

  // cert_votes_ = cert_votes
  {
    std::scoped_lock lock(cert_votes_mutex_);
    cert_votes_ = cert_votes;
    cert_voted_block_hash_ = voted_block_hash;
  }

  // unrewqaded_votes_ = cert_votes
  {
    std::scoped_lock lock(unrewarded_votes_mutex_);
    unrewarded_votes_ = std::move(cert_votes);
  }

  // new_votes_ = empty
  {
    std::scoped_lock lock(new_votes_mutex_);
    new_votes_.clear();
  }
}

std::vector<vote_hash_t> RewardsVotes::popUnrewardedVotes(size_t count) {
  {
    std::shared_lock lock(unrewarded_votes_mutex_);
    if (unrewarded_votes_.empty()) {
      return {};
    }
  }

  std::vector<vote_hash_t> selected_votes;

  {
    std::scoped_lock lock(unrewarded_votes_mutex_);

    // Can pop count votes only if there is enough votes inside unrewarded_votes_
    size_t votes_to_pop_count = std::min(count, unrewarded_votes_.size());
    selected_votes.reserve(votes_to_pop_count);

    auto it = unrewarded_votes_.begin();
    for (size_t idx = 0; idx < votes_to_pop_count; idx++) {
      assert(it != unrewarded_votes_.end());

      selected_votes.push_back(std::move(*it));
      it = unrewarded_votes_.erase(it);
    }
  }

  return selected_votes;
}

std::pair<bool, std::string> RewardsVotes::checkBlockRewardsVotes(const std::vector<vote_hash_t>& block_rewards_votes) {
  auto checkVotesContainer = [&block_rewards_votes](std::shared_mutex& mutex,
                                                    const auto& container) -> std::vector<vote_hash_t> {
    std::vector<vote_hash_t> not_found_votes;
    std::shared_lock lock(mutex);

    for (const auto& vote_hash : block_rewards_votes) {
      if (!container.contains(vote_hash)) {
        not_found_votes.push_back(vote_hash);
      }
    }

    return not_found_votes;
  };

  // Checks cert_votes_
  std::vector<vote_hash_t> not_found_votes = checkVotesContainer(cert_votes_mutex_, cert_votes_);
  if (not_found_votes.empty()) {
    return {true, ""};
  }

  // Checks new_votes_
  not_found_votes = checkVotesContainer(new_votes_mutex_, new_votes_);
  if (not_found_votes.empty()) {
    return {true, ""};
  }

  // Checks previous_period_cert_votes_ as a potential optimization before checking the database for older votes
  not_found_votes = checkVotesContainer(previous_period_cert_votes_mutex_, previous_period_cert_votes_);
  if (not_found_votes.empty()) {
    return {true, ""};
  }

  // TODO: checks BLOCK_REWARDS_VOTES db column. In case node crash, it could happen that it already saved dag block
  //       into the db and RewardsVotes is erased from memory -> any new rewards votes would be lost. Thats why all
  //       new rewards votes have to be saved into the db as soon as dag block containing them is saved into the db
  //       We could even save it directly into the PERIOD_DATA column and dont use BLOCK_REWARDS_VOTES -> depends
  //       if appending data into the PERIOD_DATA is expensive or not

  // TODO: checks PERIOD_DATA column

  std::string err_msg = "Missing votes: ";
  for (const auto& vote_hash : not_found_votes) {
    err_msg += vote_hash.abridged() + ", ";
  }

  return {false, std::move(err_msg)};
}

bool RewardsVotes::isNewVote(const vote_hash_t& vote_hash) const {
  {
    std::shared_lock lock(cert_votes_mutex_);
    if (cert_votes_.contains(vote_hash)) {
      return false;
    }
  }

  {
    std::shared_lock lock(new_votes_mutex_);
    if (new_votes_.contains(vote_hash)) {
      return false;
    }
  }

  return true;
}

bool RewardsVotes::insertNewVote(std::shared_ptr<Vote>&& new_vote) {
  const auto& vote_hash = new_vote->getHash();

  std::scoped_lock lock(new_votes_mutex_);
  return new_votes_.emplace(vote_hash, std::move(new_vote)).second;
}

}  // namespace taraxa