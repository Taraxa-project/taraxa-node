#pragma once

#include <shared_mutex>
#include <unordered_map>
#include <unordered_set>

#include "common/types.hpp"

namespace taraxa {

class Vote;

/**
 * @class RewardsVotes
 * @brief RewardsVotes contains all votes candidates for rewards from the latest finalized pbft block
 */
class RewardsVotes {
 public:
  /**
   * @brief Sets 2t+1 cert votes as well as voted block hash
   *
   * @param cert_votes
   * @param voted_block_hash
   */
  void setCertVotes(std::unordered_set<vote_hash_t>&& cert_votes, const blk_hash_t& voted_block_hash);

  /**
   * @return false in case vote is either in cert_votes_ ot new_votes_, otherwise true
   */
  bool isNewVote(const vote_hash_t& vote_hash) const;

  /**
   * @brief Inserts new vote into new_votes_ container
   *
   * @param new_vote
   * @return true if insertion actually took place, otherwise false (vote eas already inserted)
   */
  bool insertNewVote(std::shared_ptr<Vote>&& new_vote);

 private:
  // 2t+1 cert votes from the latest finalized pbft period. It is reset every time new pbft block is pushed into the
  // final chain
  std::unordered_set<vote_hash_t> cert_votes_;
  // voted pbft block hash from cert_votes_
  blk_hash_t cert_voted_block_hash_{0};
  mutable std::shared_mutex cert_votes_mutex_;

  // 2t+1 cert votes from the latest finalized pbft period votes that has not been included in dag blocks for rewards
  // yet. At the beginning (when new pbft block is pushed into the final chain) it is equal tp cert_votes_. Votes are
  // removed from this container when:
  // - node receives new dag block that includes some of these votes as candidates for rewards
  // - node creates new dag block and includes these votes in it as candidates for rewards
  std::unordered_set<vote_hash_t> unrewarded_votes_;
  mutable std::shared_mutex unrewarded_votes_mutex_;

  // New votes that has been included in dag blocks as candidates for rewards and it is not part of cert_votes_
  // These votes will be added to the latest finalized pbft period_data db column once new pbft block is finalized.
  // They have to be added to the period_data otherwise sync data would be incomplete
  std::unordered_map<vote_hash_t, std::shared_ptr<Vote>> new_votes_;
  mutable std::shared_mutex new_votes_mutex_;
};

}  // namespace taraxa
