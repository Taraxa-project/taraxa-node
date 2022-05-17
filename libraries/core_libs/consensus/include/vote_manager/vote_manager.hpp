#pragma once

#include "common/util.hpp"
#include "final_chain/final_chain.hpp"
#include "pbft/pbft_chain.hpp"
#include "vote/vote.hpp"

namespace taraxa {

class Network;

class NextVotesManager {
 public:
  NextVotesManager(addr_t node_addr, std::shared_ptr<DbStorage> db, std::shared_ptr<FinalChain> final_chain);

  void clear();

  bool find(vote_hash_t next_vote_hash);

  bool enoughNextVotes() const;

  bool haveEnoughVotesForNullBlockHash() const;

  blk_hash_t getVotedValue() const;

  std::vector<std::shared_ptr<Vote>> getNextVotes();

  size_t getNextVotesWeight() const;

  void addNextVotes(std::vector<std::shared_ptr<Vote>> const& next_votes, size_t pbft_2t_plus_1);

  void updateNextVotes(std::vector<std::shared_ptr<Vote>> const& next_votes, size_t pbft_2t_plus_1);

  void updateWithSyncedVotes(std::vector<std::shared_ptr<Vote>>& votes, size_t pbft_2t_plus_1);

  bool voteVerification(std::shared_ptr<Vote>& vote, uint64_t dpos_period, size_t dpos_total_votes_count,
                        size_t pbft_sortition_threshold);

 private:
  using UniqueLock = boost::unique_lock<boost::shared_mutex>;
  using SharedLock = boost::shared_lock<boost::shared_mutex>;
  using UpgradableLock = boost::upgrade_lock<boost::shared_mutex>;
  using UpgradeLock = boost::upgrade_to_unique_lock<boost::shared_mutex>;

  void assertError_(std::vector<std::shared_ptr<Vote>> next_votes_1,
                    std::vector<std::shared_ptr<Vote>> next_votes_2) const;

  mutable boost::shared_mutex access_;

  std::shared_ptr<DbStorage> db_;
  std::shared_ptr<FinalChain> final_chain_;

  bool enough_votes_for_null_block_hash_;
  blk_hash_t voted_value_;  // For value is not null block hash
  // <voted PBFT block hash, next votes list that have exactly 2t+1 votes voted at the PBFT block hash>
  // only save votes == 2t+1 voted at same value in map and set
  std::unordered_map<blk_hash_t, std::vector<std::shared_ptr<Vote>>> next_votes_;
  std::unordered_map<blk_hash_t, uint64_t> next_votes_weight_;
  std::unordered_set<vote_hash_t> next_votes_set_;

  LOG_OBJECTS_DEFINE
};

class VoteManager {
 public:
  VoteManager(size_t pbft_committee_size, const addr_t& node_addr, std::shared_ptr<DbStorage> db,
              std::shared_ptr<PbftChain> pbft_chain, std::shared_ptr<FinalChain> final_chain,
              std::shared_ptr<NextVotesManager> next_votes_mgr);
  ~VoteManager();
  VoteManager(const VoteManager&) = delete;
  VoteManager(VoteManager&&) = delete;
  VoteManager& operator=(const VoteManager&) = delete;
  VoteManager& operator=(VoteManager&&) = delete;

  void setNetwork(std::weak_ptr<Network> network);

  // Unverified votes
  bool addUnverifiedVote(std::shared_ptr<Vote> const& vote);
  void moveVerifyToUnverify(std::vector<std::shared_ptr<Vote>> const& votes);
  void removeUnverifiedVote(uint64_t pbft_round, vote_hash_t const& vote_hash);
  bool voteInUnverifiedMap(uint64_t pbft_round, vote_hash_t const& vote_hash);
  std::vector<std::shared_ptr<Vote>> getUnverifiedVotes();
  void clearUnverifiedVotesTable();
  uint64_t getUnverifiedVotesSize() const;

  // Verified votes
  void addVerifiedVote(std::shared_ptr<Vote> const& vote);
  bool voteInVerifiedMap(std::shared_ptr<Vote> const& vote);
  void clearVerifiedVotesTable();
  std::vector<std::shared_ptr<Vote>> getVerifiedVotes();
  uint64_t getVerifiedVotesSize() const;

  void removeVerifiedVotes();

  void verifyVotes(uint64_t pbft_round, std::function<bool(std::shared_ptr<Vote> const&)> const& is_valid);

  void cleanupVotes(uint64_t pbft_round);

  std::string getJsonStr(std::vector<std::shared_ptr<Vote>> const& votes);

  std::vector<std::shared_ptr<Vote>> getProposalVotes(uint64_t pbft_round);

  std::optional<VotesBundle> getVotesBundleByRoundAndStep(uint64_t round, size_t step, size_t two_t_plus_one);

  uint64_t roundDeterminedFromVotes(size_t two_t_plus_one);

  // Reward votes

  /**
   * @brief Send reward period cert votes to peers. Reward period cert votes should be greater or equal to reward votes
   *
   * @param reward_period
   */
  void sendRewardPeriodCertVotes(uint64_t reward_period);

  /**
   * @brief Add unverified incoming reward vote to reward votes table
   *
   * @param vote unverified incoming reward vote
   *
   * @return true if add successful
   */
  bool addRewardVote(const std::shared_ptr<Vote>& vote);

  /**
   * @brief Verify reward vote
   *
   * @param vote reward vote
   *
   * @return true if pass vote verification
   */
  bool verifyRewardVote(const std::shared_ptr<Vote>& vote);

  /**
   * @brief Update missing reward votes to previous period finalized block
   *
   * @param reward_period
   *
   * @return reward votes hashes that are same as previous period cert votes hashes
   */
  std::vector<vote_hash_t> updateRewardVotesAndGetBackRewardPeriodCertVotesHashes(uint64_t reward_period);

  /**
   * @brief Check previous finalized block that includes all reward votes
   *
   * @param pbft_block
   * @param reward_period_cert_votes_hashes
   *
   * @return true if include all reward votes
   */
  bool checkRewardVotes(const std::shared_ptr<PbftBlock>& pbft_block,
                        std::vector<vote_hash_t>&& reward_period_cert_votes_hashes);

 private:
  void retreieveVotes_();

  using UniqueLock = boost::unique_lock<boost::shared_mutex>;
  using SharedLock = boost::shared_lock<boost::shared_mutex>;
  using UpgradableLock = boost::upgrade_lock<boost::shared_mutex>;
  using UpgradeLock = boost::upgrade_to_unique_lock<boost::shared_mutex>;

  // <pbft round, <vote hash, vote>>
  std::map<uint64_t, std::unordered_map<vote_hash_t, std::shared_ptr<Vote>>> unverified_votes_;

  // <PBFT round, <PBFT step, <voted value, pair<voted weight, <vote hash, vote>>>>
  std::map<
      uint64_t,
      std::map<size_t, std::unordered_map<blk_hash_t,
                                          std::pair<uint64_t, std::unordered_map<vote_hash_t, std::shared_ptr<Vote>>>>>>
      verified_votes_;

  std::unordered_set<vote_hash_t> votes_invalid_in_current_final_chain_period_;
  blk_hash_t current_period_final_chain_block_hash_;
  std::unordered_map<addr_t, uint64_t> max_received_round_for_address_;

  std::unique_ptr<std::thread> daemon_;

  mutable boost::shared_mutex unverified_votes_access_;
  mutable boost::shared_mutex verified_votes_access_;

  const size_t pbft_committee_size_;
  const addr_t node_addr_;

  std::shared_ptr<DbStorage> db_;
  std::shared_ptr<PbftChain> pbft_chain_;
  std::shared_ptr<FinalChain> final_chain_;
  std::shared_ptr<NextVotesManager> next_votes_manager_;
  std::weak_ptr<Network> network_;

  std::pair<blk_hash_t, std::unordered_map<vote_hash_t, std::shared_ptr<Vote>>> reward_votes_;
  mutable std::shared_mutex reward_votes_mutex_;

  LOG_OBJECTS_DEFINE
};

}  // namespace taraxa
