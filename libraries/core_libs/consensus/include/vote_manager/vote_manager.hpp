#pragma once

#include "common/util.hpp"
#include "final_chain/final_chain.hpp"
#include "key_manager/key_manager.hpp"
#include "pbft/pbft_chain.hpp"
#include "vote/vote.hpp"

namespace taraxa {

/** @addtogroup Vote
 * @{
 */

class Network;

/**
 * @brief NextVotesManager class manage next voting type votes for previous PBFT round.
 *
 * Node could send previous PBFT round next voting type votes to help peers catch up to the current PBFT round. Also,
 * node could receive enough next voting type votes from peers,  to help node itself catch up to the correct PBFT round
 * in consensus.
 */
class NextVotesManager {
 public:
  NextVotesManager(addr_t node_addr, std::shared_ptr<DbStorage> db, std::shared_ptr<FinalChain> final_chain,
                   std::shared_ptr<KeyManager> key_manager);

  /**
   * @brief Clear previous PBFT round next voting type votes
   */
  void clear();

  /**
   * @brief Check if the vote has been in the map
   * @param next_vote_hash next voting type vote hash
   * @return true if found
   */
  bool find(vote_hash_t next_vote_hash);

  /**
   * @brief Check if exist enough next voting type votes
   * @return true if there are enough next voting type votes
   */
  bool enoughNextVotes() const;

  /**
   * @brief Check if exist enough next voting type votes for PBFT NULL block hash
   * @return true if there are enough next voting type votes
   */
  bool haveEnoughVotesForNullBlockHash() const;

  /**
   * @brief Get next voting type votes vote value
   * @return next voting type votes vote value
   */
  blk_hash_t getVotedValue() const;

  /**
   * @brief Get previous PBFT round all next voting type votes
   * @return previous PBFT round all next voting type votes
   */
  std::vector<std::shared_ptr<Vote>> getNextVotes();

  /**
   * @brief Get total weight of previous PBFT round all next voting type votes
   * @return total weight of previous PBFT round all next voting type votes
   */
  size_t getNextVotesWeight() const;

  /**
   * @brief Add a bunch of next voting type votes to the map
   * @param next_votes next voting type votes
   * @param pbft_2t_plus_1 PBFT 2t+1 is 2/3 of PBFT sortition threshold and plus 1
   */
  void addNextVotes(std::vector<std::shared_ptr<Vote>> const& next_votes, size_t pbft_2t_plus_1);

  /**
   * @brief Update a bunch of next voting type votes to the map
   * @param next_votes next voting type votes
   * @param pbft_2t_plus_1 PBFT 2t+1 is 2/3 of PBFT sortition threshold and plus 1
   */
  void updateNextVotes(std::vector<std::shared_ptr<Vote>> const& next_votes, size_t pbft_2t_plus_1);

  /**
   * @brief Update a bunch of next voting type votes that synced from peers to the map
   * @param votes next voting type votes
   * @param pbft_2t_plus_1 PBFT 2t+1 is 2/3 of PBFT sortition threshold and plus 1
   */
  void updateWithSyncedVotes(std::vector<std::shared_ptr<Vote>>& votes, size_t pbft_2t_plus_1);

  /**
   * @brief Verify vote validation
   * @param vote vote
   * @param dpos_period DPOS period
   * @param dpos_total_votes_count total DPOS votes count
   * @param pbft_sortition_threshold PBFT sortition threshold is minimum of between PBFT committee size and total DPOS
   * votes count
   * @return true if passed validation
   */
  bool voteVerification(std::shared_ptr<Vote>& vote, uint64_t dpos_period, size_t dpos_total_votes_count,
                        size_t pbft_sortition_threshold);

 private:
  using UniqueLock = boost::unique_lock<boost::shared_mutex>;
  using SharedLock = boost::shared_lock<boost::shared_mutex>;
  using UpgradableLock = boost::upgrade_lock<boost::shared_mutex>;
  using UpgradeLock = boost::upgrade_to_unique_lock<boost::shared_mutex>;

  /**
   * @brief Assertion happens when there is more than 1 voting value for a non-NULL block hash
   * @param next_votes_1 a bunch of next voting type votes vote on a non-NULL block hash
   * @param next_votes_2 a bunch of next voting type votes vote on another non-NULL block hash
   */
  void assertError_(std::vector<std::shared_ptr<Vote>> next_votes_1,
                    std::vector<std::shared_ptr<Vote>> next_votes_2) const;

  mutable boost::shared_mutex access_;

  std::shared_ptr<DbStorage> db_;
  std::shared_ptr<FinalChain> final_chain_;
  std::shared_ptr<KeyManager> key_manager_;

  bool enough_votes_for_null_block_hash_;
  blk_hash_t voted_value_;  // For value is not null block hash
  // <voted PBFT block hash, next votes list that have exactly 2t+1 votes voted at the PBFT block hash>
  // only save votes == 2t+1 voted at same value in map and set
  std::unordered_map<blk_hash_t, std::vector<std::shared_ptr<Vote>>> next_votes_;
  std::unordered_map<blk_hash_t, uint64_t> next_votes_weight_;
  std::unordered_set<vote_hash_t> next_votes_set_;

  LOG_OBJECTS_DEFINE
};

/**
 * @brief VoteManager class manage votes for PBFT consensus
 */
class VoteManager {
 public:
  VoteManager(size_t pbft_committee_size, const addr_t& node_addr, std::shared_ptr<DbStorage> db,
              std::shared_ptr<PbftChain> pbft_chain, std::shared_ptr<FinalChain> final_chain,
              std::shared_ptr<NextVotesManager> next_votes_mgr, std::shared_ptr<KeyManager> key_manager);
  ~VoteManager();
  VoteManager(const VoteManager&) = delete;
  VoteManager(VoteManager&&) = delete;
  VoteManager& operator=(const VoteManager&) = delete;
  VoteManager& operator=(VoteManager&&) = delete;

  /**
   * @brief Set network as a weak pointer
   * @param network a weak pinter
   */
  void setNetwork(std::weak_ptr<Network> network);

  // Unverified votes
  /**
   * @brief Add a vote to the unverified votes map
   * @param vote vote
   * @return true if added
   */
  bool addUnverifiedVote(std::shared_ptr<Vote> const& vote);

  /**
   * @brief Add verified votes to the unverified votes map
   * @param votes verified votes
   */
  void moveVerifyToUnverify(std::vector<std::shared_ptr<Vote>> const& votes);

  /**
   * @brief Remove an unverified vote
   * @param pbft_round vote PBFT round
   * @param vote_hash vote hash
   */
  void removeUnverifiedVote(uint64_t pbft_round, vote_hash_t const& vote_hash);

  /**
   * @brief Check if the vote has been in the unverified votes map
   * @param pbft_round vote PBFT round
   * @param vote_hash vote hash
   * @return true if exist
   */
  bool voteInUnverifiedMap(uint64_t pbft_round, vote_hash_t const& vote_hash);

  /**
   * @brief Get all unverified votes
   * @return all unverified votes
   */
  std::vector<std::shared_ptr<Vote>> copyUnverifiedVotes();

  /**
   * @brief Clear the unverified votes map
   */
  void clearUnverifiedVotesTable();

  /**
   * @brief Get the total size of all unverified votes
   * @return the total size of all unverified votes
   */
  uint64_t getUnverifiedVotesSize() const;

  // Verified votes
  /**
   * @brief Add a vote to the verified votes map
   * @param vote vote
   */
  void addVerifiedVote(std::shared_ptr<Vote> const& vote);

  /**
   * @brief Check if the vote has been in the verified votes map
   * @param vote vote
   * @return true if exist
   */
  bool voteInVerifiedMap(std::shared_ptr<Vote> const& vote);

  /**
   * @brief Clear the verified votes map
   */
  void clearVerifiedVotesTable();

  /**
   * @brief Get all verified votes
   * @return all verified votes
   */
  std::vector<std::shared_ptr<Vote>> getVerifiedVotes();

  /**
   * @brief Get the total size of all verified votes
   * @return the total size of all verified votes
   */
  uint64_t getVerifiedVotesSize() const;

  /**
   * @brief Move all verified votes to the unverified map. Since PBFT chain has pushed new blocks, that will affect DPOS
   * eligible votes count and players' eligibility
   */
  void removeVerifiedVotes();

  /**
   * @brief Cleanup votes for previous PBFT rounds and verify all unverified votes
   * @param pbft_round current PBFT round
   * @param is_valid vote validation function
   */
  void verifyVotes(uint64_t pbft_round, std::function<bool(std::shared_ptr<Vote> const&)> const& is_valid);

  /**
   * @brief Cleanup votes for previous PBFT rounds
   * @param pbft_round current PBFT round
   */
  void cleanupVotes(uint64_t pbft_round);

  /**
   * @brief Get all verified votes in proposal vote type for the current PBFT round
   * @param pbft_round current PBFT round
   * @return all verified votes in proposal vote type for the current PBFT round
   */
  std::vector<std::shared_ptr<Vote>> getProposalVotes(uint64_t pbft_round);

  /**
   * @brief Get a bunch of votes that vote on the same voting value in the specific PBFT round and step, the total votes
   * weights must be greater or equal to PBFT 2t+1
   * @param round PBFT round
   * @param step PBFT step
   * @param two_t_plus_one PBFT 2t+1 is 2/3 of PBFT sortition threshold and plus 1
   * @return VotesBundle a bunch of votes that vote on the same voting value in the specific PBFT round and step
   */
  std::optional<VotesBundle> getVotesBundleByRoundAndStep(uint64_t round, size_t step, size_t two_t_plus_one);

  /**
   * @brief Check if there are enough next voting type votes to set PBFT to a forward round
   * @param two_t_plus_one PBFT 2t+1 is 2/3 of PBFT sortition threshold and plus 1
   * @return a new PBFT round number if there are enough next voting type votes. Otherwise return 0
   */
  uint64_t roundDeterminedFromVotes(size_t two_t_plus_one);

  // reward votes

  /**
   * @brief Add last period cert vote to reward_votes_ after the cert vote voted block finalized
   *
   * @param cert vote voted to last period PBFT block
   *
   * @return true if add successful
   */
  bool addRewardVote(const std::shared_ptr<Vote>& vote);

  /**
   * @brief Check reward_votes_ if including all reward votes for the PBFT block
   *
   * @param PBFT block
   *
   * @return true if include all reward votes
   */
  bool checkRewardVotes(const std::shared_ptr<PbftBlock>& pbft_block);

  /**
   * @brief When finalize a new PBFT block, clear reward_votes_ and add the new cert votes to reward_votes_
   *
   * @param cert votes for last finalized PBFT block
   */
  void replaceRewardVotes(std::vector<std::shared_ptr<Vote>>&& cert_votes);

  /**
   * @brief Get all reward votes in reward_votes_
   *
   * @return vector of all reward votes
   */

  std::vector<std::shared_ptr<Vote>> getRewardVotes();

  /**
   * @brief Send out all reward votes to peers
   *
   * @param PBFT block hash
   */
  void sendRewardVotes(const blk_hash_t& pbft_block_hash);

 private:
  /**
   * @brief Verify reward vote
   *
   * @param cert vote voted to last period PBFT block
   *
   * @return true if pass vote verification
   */
  bool verifyRewardVote(const std::shared_ptr<Vote>& vote);

  /**
   * @brief Retrieve all verified votes from DB to the verified votes map. And broadcast all next voting type votes to
   * peers if node has extended the partition steps (1000). That only happens when nodes start.
   */
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
  std::shared_ptr<KeyManager> key_manager_;
  std::weak_ptr<Network> network_;

  blk_hash_t reward_votes_pbft_block_hash_;
  uint64_t reward_votes_pbft_block_round_;
  std::unordered_map<vote_hash_t, std::shared_ptr<Vote>> reward_votes_;
  mutable std::shared_mutex reward_votes_mutex_;

  LOG_OBJECTS_DEFINE
};

/** @}*/

}  // namespace taraxa
