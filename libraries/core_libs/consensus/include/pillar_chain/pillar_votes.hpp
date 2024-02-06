#pragma once

#include <memory>
#include <shared_mutex>
#include "vote/pillar_vote.hpp"

namespace taraxa::pillar_chain {

class PillarVotes {
 public:
  struct WeightVotes {
    std::unordered_map<vote_hash_t, std::shared_ptr<PillarVote>> votes;
    uint64_t weight{0};  // votes weight
  };

  struct PeriodVotes {
    std::unordered_map<blk_hash_t, WeightVotes> pillar_block_votes;
    std::unordered_map<addr_t, vote_hash_t> unique_voters;

    // 2t+1 threshold for pillar block period
    uint64_t two_t_plus_one{0};
  };

 public:
  /**
   * @brief Checks if vote exists
   *
   * @param vote
   * @return true if exists, otherwise false
   */
  bool voteExists(const std::shared_ptr<PillarVote> vote) const;

  /**
   * @brief Checks if vote is unique per period & validator
   *
   * @param vote
   * @return true if unique, otherwise false
   */
  bool isUniqueVote(const std::shared_ptr<PillarVote> vote) const;

  /**
   * @brief Checks if there is 2t+1 votes for specified period
   *
   * @param period
   * @return
   */
  bool hasTwoTPlusOneVotes(PbftPeriod period, const blk_hash_t& block_hash) const;

  /**
   * @brief Checks if specified period data have been already initialized
   *
   * @param period
   * @return true if initialized, othwrwise false
   */
  bool periodDataInitialized(PbftPeriod period) const;

  /**
   * @brief Initialize period data with period_two_t_plus_one
   *
   * @param period
   * @param period_two_t_plus_one
   */
  void initializePeriodData(PbftPeriod period, uint64_t period_two_t_plus_one);

  /**
   * @brief Add a vote to the votes map
   * @param vote vote
   * @param validator_vote_count
   *
   * @return true if vote was successfully added, otherwise false
   */
  bool addVerifiedVote(const std::shared_ptr<PillarVote>& vote, u_int64_t validator_vote_count);

  /**
   * @brief Get all pillar block votes for specified pillar block
   *
   * @param period
   * @param pillar_block_hash
   * @param two_t_plus_one if true, return only if there is >= 2t+1 verified votes
   *
   * @return all pillar block votes for specified period and pillar block hash
   */
  std::vector<std::shared_ptr<PillarVote>> getVerifiedVotes(PbftPeriod period,
                                                                   const blk_hash_t& pillar_block_hash,
                                                                   bool two_t_plus_one = false) const;

  /**
   * @brief Erases votes wit period < min_period
   *
   * @param min_period
   */
  void eraseVotes(PbftPeriod min_period);

 private:
  // Votes for latest_pillar_block_.period - 1, latest_pillar_block_.period and potential +1 future pillar
  // block period
  std::map<PbftPeriod, PeriodVotes> votes_;
  mutable std::shared_mutex mutex_;
};

}  // namespace taraxa::pillar_chain