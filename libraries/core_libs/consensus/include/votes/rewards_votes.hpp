#pragma once

#include <shared_mutex>
#include <unordered_set>
#include <unordered_map>
#include "common/types.hpp"

namespace taraxa {

class Vote;

/**
 * @class RewardsVotes
 * @brief RewardsVotes contains rewards all votes for current_pbft_period - 1 period that should be rewarded
 *
 * @note This class stores only cert votes for current_pbft_period - 1
 */
class RewardsVotes {
 public:

  /**
   * @return false in case vote is either in two_t_plus_1_votes_ ot new_votes_, otherwise true
   */
  bool isNewVote() const;

  /**
   * @brief Inserts new vote into new_votes_ container
   *
   * @param new_vote
   */
  void insertNewVote(std::shared_ptr<Vote>&& new_vote);
 private:
  // 2t+1 cert votes from previous finalized pbft period. It is reset every time new pbft block is pushed into the
  // final chain
  std::unordered_set<vote_hash_t> two_t_plus_1_votes_;
  mutable std::shared_mutex two_t_plus_1_votes_mutex_;

  // 2t+1 cert votes from previous finalized pbft period votes that has not been included in dag blocks for rewards yet.
  // At the beginning (when new pbft block is pushed into the final chain) it is equal tp two_t_plus_1_votes_.
  // Votes are removed from this container when:
  // - node receives new dag block that includes some of these votes as candidates for rewards
  // - node creates new dag block and includes these votes in it as candidates for rewards
  std::unordered_set<vote_hash_t> unrewarded_votes_;
  mutable std::shared_mutex unrewarded_votes_mutex_;

  // New votes that has been included in dag blocks as candidates for rewards and it is not part of two_t_plus_1_votes_
  // These votes will be added to the current_pbft_period - 1 period_data db column once current_pbft_period is finalized.
  // They have to be added to the period_data otherwise sync data would be incomplete
  std::unordered_map<vote_hash_t, std::shared_ptr<Vote>> new_votes_;
  mutable std::shared_mutex new_votes_mutex_;
};

}  // namespace taraxa
