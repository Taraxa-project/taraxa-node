#include "votes/rewards_votes.hpp"

#include <mutex>

#include "vote/vote.hpp"

namespace taraxa {

void RewardsVotes::setCertVotes(std::unordered_set<vote_hash_t>&& cert_votes, const blk_hash_t& voted_block_hash) {
  std::scoped_lock lock(cert_votes_mutex_);
  cert_votes_ = std::move(cert_votes);
  cert_voted_block_hash_ = voted_block_hash;
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