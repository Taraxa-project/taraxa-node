#pragma once

#include <map>
#include <shared_mutex>
#include <unordered_set>

#include "common/types.hpp"

namespace taraxa {

class DbStorage;
class Vote;

/**
 * @class RewardsVotes
 * @brief RewardsVotes contains all votes candidates for rewards from the latest finalized pbft block
 */
class RewardsVotes {
 public:
  RewardsVotes(std::shared_ptr<DbStorage> db, uint64_t last_saved_pbft_period);
  ~RewardsVotes() = default;

  /**
   * @brief Stores new pbft finalized block cert votes and clears unrewarded_votes_
   *
   * @param pbft_cert_votes 2t+1 cert votes observed for latest pbft period
   * @param voted_pbft_block_hash hash of the latest cert voted pbft block
   * @param pbft_rewards_votes all unique rewards votes included in dag block from voted_pbft_block_hash pbft block
   */
  void newPbftBlockFinalized(std::unordered_set<vote_hash_t>&& pbft_cert_votes, const blk_hash_t& voted_pbft_block_hash,
                             const std::unordered_set<vote_hash_t>& pbft_rewards_votes);

  /**
   * @brief Pops count votes from unrewarded_votes_. If count > unrewarded_votes_.size(), unrewarded_votes_.size()
   *        number of votes are popped
   *
   * @param count
   * @return vector<vote_hash_t>
   */
  std::vector<vote_hash_t> popUnrewardedVotes(size_t count);

  /**
   * @brief Removes votes from unrewarded_votes_.
   * @note It is called when new dag block is added into the dag
   *
   * @param votes
   */
  void removeUnrewardedVotes(const std::vector<vote_hash_t>& votes);

  /**
   * @brief Checks if all block rewards votes are present (are part of 2t+1 cert votes or were received
   *        as new votes through VotesDagSyncPacket)
   *
   * @note No need to verify these votes as RewardsVotes contains only verified votes
   * @param block_rewards_votes block rewards votes
   * @return <true, ""> in case all block rewards votes are present, <false, "err message"> in case some of them are
   * missing
   */
  std::pair<bool, std::string> checkBlockRewardsVotes(const std::vector<vote_hash_t>& block_rewards_votes);

  /**
   * @brief Returns only those dag_block_rewards_votes that are also part of new_votes_
   *
   * @param dag_block_rewards_votes
   * @return filtered new votes from dag_block_rewards_votes
   */
  std::vector<std::shared_ptr<Vote>> filterNewVotes(const std::vector<vote_hash_t>& dag_block_rewards_votes);

  /**
   * @brief Moves votes dag_block_new_rewards_votes (must be also part of new_votes_) into the new_processed_votes_
   *
   * @note It is caller's responsibility to make sure that new_processed_votes_ == db column new_rewards_votes so before
   *       calling this function, dag_block_new_rewards_votes should be already part of new_rewards_votes db column
   * @param dag_block_new_rewards_votes
   */
  void markNewVotesAsProcessed(const std::vector<std::shared_ptr<Vote>>& dag_block_new_rewards_votes);

  /**
   * @return true if vote is known (either in db or RewardsVotes internal structures), otherwise false
   */
  bool isKnownVote(const std::shared_ptr<Vote>& vote) const;

  /**
   * @brief Inserts new vote into new_votes_ container
   *
   * @param new_vote
   * @return true if insertion actually took place, otherwise false (vote eas already inserted)
   */
  bool insertVote(std::shared_ptr<Vote>&& new_vote);

 private:
  /**
   * @brief Returns only those dag_blocks_rewards_votes that are also part of new_processed_votes_
   *
   * @param dag_blocks_rewards_votes
   * @return filtered new processed votes from dag_blocks_rewards_votes
   */
  std::vector<std::shared_ptr<Vote>> filterNewProcessedVotes(
      const std::unordered_set<vote_hash_t>& dag_blocks_rewards_votes);

  /**
   * @brief Removes votes_to_be_removed from new_processed_votes_
   *
   * @note It is caller's responsibility to make sure that new_processed_votes_ == db column new_rewards_votes so before
   *       calling this function, votes_to_be_removed should be already removed from new_rewards_votes db column
   * @param votes_to_be_removed
   */
  void removeNewProcessedVotes(const std::vector<std::shared_ptr<Vote>>& votes_to_be_removed);

  /**
   * @brief Appends new votes that have been finalized through dag blocks included in pbft block into the cert_votes_
   * @note It is called when new pbft block is finalized
   *
   * @param periods_votes
   */
  void appendNewVotesToCertVotes(
      const std::unordered_map<blk_hash_t, std::vector<std::shared_ptr<Vote>>>& blocks_votes);

 private:
  std::shared_ptr<DbStorage> db_;

  // How many periods for cert votes are cached
  const size_t k_max_cached_periods_count{10};

  // Observed <voted_block_hash, 2t+1 cert votes> for the last N finalized pbft periods. It is shifted
  // (blocks_cert_votes_[N] = blocks_cert_votes_[N-1], ...) every time new pbft block is pushed into the final chain
  std::map<blk_hash_t, std::unordered_set<vote_hash_t>> blocks_cert_votes_;
  mutable std::shared_mutex blocks_cert_votes_mutex_;

  // 2t+1 cert votes from the latest finalized pbft period votes that has not been included in dag blocks for rewards
  // yet. At the beginning (when new pbft block is pushed into the final chain) it is equal tp cert_votes_. Votes are
  // removed from this container when:
  // - new dag block that includes some of these votes as candidates for rewards is added to the dag
  // - node creates new dag block and includes these votes in it as candidates for rewards
  std::unordered_set<vote_hash_t> unrewarded_votes_;
  mutable std::shared_mutex unrewarded_votes_mutex_;

  // New votes that have been received through VotesDagSyncPacket but no DagBlock that would include them was yet
  // processed. Once dag block that contains them is added to dag, such votes are moved to new_processed_votes_ and
  // added to the new_rewards_votes db column
  std::unordered_map<vote_hash_t, std::shared_ptr<Vote>> new_votes_;
  mutable std::shared_mutex new_votes_mutex_;

  // New votes (not part of observed 2t+1 votes) that have been received through VotesDagSyncPacket and were already
  // processed through dag blocks that include them and are already part of dag.
  // new_processed_votes_ should be always equal to db column new_rewards_votes
  // These votes will be added to the period_data and cert_votes_period db columns once new pbft block (that contains
  // dag blocks that contain these votes) is finalized.
  std::unordered_map<vote_hash_t, std::shared_ptr<Vote>> new_processed_votes_;
  mutable std::shared_mutex new_processed_votes_mutex_;
};

}  // namespace taraxa
