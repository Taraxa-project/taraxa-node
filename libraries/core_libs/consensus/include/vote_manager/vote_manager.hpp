#pragma once

#include "common/util.hpp"
#include "common/vrf_wrapper.hpp"
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
  NextVotesManager(addr_t node_addr, std::shared_ptr<DbStorage> db, std::shared_ptr<FinalChain> final_chain);

  /**
   * @brief Clear previous PBFT round next voting type votes
   */
  void clearVotes();

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
  // CONCERN: This is only used in tests, and doesn't make any sense to me to have in general
  //          because "enough next votes" is a totally undefined concept in the protocol...
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
  std::optional<blk_hash_t> getVotedValue() const;

  /**
   * @brief Get previous PBFT round all next voting type votes
   * @return previous PBFT round all next voting type votes
   */
  std::vector<std::shared_ptr<Vote>> getNextVotes();

  /**
   * @brief Get total weight of all previous PBFT round next votes
   * @return total weight of all previous PBFT round all next votes
   */
  size_t getNextVotesWeight() const;

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
   * @brief Add a bunch of next voting type votes to the map
   * @param next_votes next voting type votes
   * @param pbft_2t_plus_1 PBFT 2t+1 is 2/3 of PBFT sortition threshold and plus 1
   */
  void addNextVotes(std::vector<std::shared_ptr<Vote>> const& next_votes, size_t pbft_2t_plus_1);

 private:
  using UniqueLock = boost::unique_lock<boost::shared_mutex>;
  using SharedLock = boost::shared_lock<boost::shared_mutex>;

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

  bool enough_votes_for_null_block_hash_;
  std::optional<blk_hash_t> voted_value_;

  // TODO[2000]: merge these structures into one
  // <voted PBFT block hash, next votes list that have exactly 2t+1 votes voted at the PBFT block hash>
  // only save votes == 2t+1 voted at same value in map and set
  std::unordered_map<blk_hash_t, std::vector<std::shared_ptr<Vote>>> next_votes_;
  std::unordered_map<blk_hash_t, uint64_t> next_votes_weight_;
  std::unordered_set<vote_hash_t> next_votes_set_;

  LOG_OBJECTS_DEFINE
};

// TODO[1907]: refactor vote manager
/**
 * @brief VoteManager class manage votes for PBFT consensus
 */
class VoteManager {
 public:
  VoteManager(const addr_t& node_addr, const PbftConfig& pbft_config, const secret_t& node_sk,
              const vrf_wrapper::vrf_sk_t& vrf_sk, std::shared_ptr<DbStorage> db, std::shared_ptr<PbftChain> pbft_chain,
              std::shared_ptr<FinalChain> final_chain, std::shared_ptr<KeyManager> key_manager);
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

  // Verified votes
  /**
   * @brief Add a vote to the verified votes map
   * @param vote vote
   *
   * @return true if vote was successfully added, otherwise false
   */
  bool addVerifiedVote(std::shared_ptr<Vote> const& vote);

  /**
   * @brief Check if the vote has been in the verified votes map
   * @param vote vote
   * @return true if exist
   */
  bool voteInVerifiedMap(std::shared_ptr<Vote> const& vote) const;

  /**
   * @brief Inserts unique vote
   * @param vote
   * @return true if vote was successfully inserted(it was unique) or this specific vote was already inserted, otherwise
   * false
   */
  bool insertUniqueVote(const std::shared_ptr<Vote>& vote);

  /**
   * @param vote
   * @return <true, ""> if vote is unique per round & step & voter, otherwise <false, "err msg">
   */
  std::pair<bool, std::string> isUniqueVote(const std::shared_ptr<Vote>& vote) const;

  /**
   * @brief Get all verified votes
   * @return all verified votes
   */
  std::vector<std::shared_ptr<Vote>> getVerifiedVotes() const;

  /**
   * @brief Get the total size of all verified votes
   * @return the total size of all verified votes
   */
  uint64_t getVerifiedVotesSize() const;

  /**
   * @brief Cleanup votes for previous PBFT rounds
   * @param pbft_period current PBFT period
   * @param pbft_round current PBFT round
   */
  void cleanupVotesByRound(PbftPeriod pbft_period, PbftRound pbft_round);

  /**
   * @brief Cleanup votes for previous PBFT periods
   * @param pbft_period current PBFT period
   */
  void cleanupVotesByPeriod(PbftPeriod pbft_period);

  /**
   * @brief Get single propose vote for specified voted block
   * @param period
   * @param round
   * @param voted_block_hash
   * @return single propose vote for specified voted block
   */
  std::shared_ptr<Vote> getProposalVote(PbftPeriod period, PbftRound round, const blk_hash_t& voted_block_hash) const;

  /**
   * @brief Get all verified votes in proposal vote type for the current PBFT round
   * @param period new PBFT period (period == chain_size + 1)
   * @param round current PBFT round
   * @return all verified votes in proposal vote type for the current PBFT round
   */
  std::vector<std::shared_ptr<Vote>> getProposalVotes(PbftPeriod period, PbftRound round) const;

  /**
   * @brief Get a bunch of votes that vote on the same voting value in the specific PBFT round and step, the total votes
   * weights must be greater or equal to PBFT 2t+1
   * @param period PBFT period
   * @param round PBFT round
   * @param step PBFT step
   * @param two_t_plus_one PBFT 2t+1 is 2/3 of PBFT sortition threshold and plus 1
   * @return VotesBundle a bunch of votes that vote on the same voting value in the specific PBFT round and step
   */
  std::optional<VotesBundle> getTwoTPlusOneVotesBundle(PbftPeriod period, PbftRound round, PbftStep step,
                                                       uint64_t two_t_plus_one) const;

  /**
   * @brief Check if there are enough next voting type votes to set PBFT to a forward round within period
   * @param period is current pbft period
   * @param two_t_plus_one PBFT 2t+1 is 2/3 of PBFT sortition threshold and plus 1
   * @return optional<pair<new round, 2t+1 next votes>> if there is enough next votes from prior round, otherwise
   * returns empty optional
   */
  std::optional<std::pair<PbftRound, std::vector<std::shared_ptr<Vote>>>> determineRoundFromPeriodAndVotes(
      PbftPeriod period, uint64_t two_t_plus_one);

  // reward votes
  /**
   * @return current rewards votes <pbft block hash, pbft block period>
   */
  std::pair<blk_hash_t, PbftPeriod> getCurrentRewardsVotesBlock() const;

  /**
   * @brief Add last period cert vote to reward_votes_ after the cert vote voted block finalized
   * @param cert vote voted to last period PBFT block
   *
   * @return true if vote was successfully added, otherwise false
   */
  bool addRewardVote(const std::shared_ptr<Vote>& vote);

  /**
   * @param vote_hash
   * @return true if vote_hash is already in rewards_votes, otheriwse false
   */
  bool isInRewardsVotes(const vote_hash_t& vote_hash) const;

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
   *
   * @return old reward votes
   */
  std::unordered_map<vote_hash_t, std::shared_ptr<Vote>> replaceRewardVotes(
      const std::vector<std::shared_ptr<Vote>>& cert_votes);

  /**
   * @brief Get all reward votes in reward_votes_
   *
   * @return vector of all reward votes
   */
  std::vector<std::shared_ptr<Vote>> getAllRewardVotes();

  /**
   * @brief Get reward votes from specified hashes
   *
   * @param vote_hashes votes hashes to retrieve
   * @return reward votes, if any of the votes is missing an empty array is returned
   */
  std::vector<std::shared_ptr<Vote>> getRewardVotesByHashes(const std::vector<vote_hash_t>& vote_hashes);

  /**
   * @brief Get reward votes from reward_votes_ with the round during which was the previous block pushed
   *
   * @return vector of reward votes
   */
  std::vector<std::shared_ptr<Vote>> getProposeRewardVotes();

  /**
   * @brief Send out all reward votes to peers
   *
   * @param PBFT block hash
   * @param rebroadcast if rebroadcast is true, all votes are resent to all peers
   */
  void sendRewardVotes(const blk_hash_t& pbft_block_hash, bool rebroadcast = false);

  /**
   * @brief Get current reward votes pbft block period
   *
   * @return period
   */
  uint64_t getRewardVotesPbftBlockPeriod();

  /**
   * @brief Place a vote, save it in the verified votes queue, and gossip to peers
   * @param blockhash vote on PBFT block hash
   * @param vote_type vote type
   * @param period PBFT period
   * @param round PBFT round
   * @param step PBFT step
   * @param step PBFT step
   */
  std::shared_ptr<Vote> generateVoteWithWeight(const blk_hash_t& blockhash, PbftVoteTypes vote_type, PbftPeriod period,
                                               PbftRound round, PbftStep step);

  /**
   * @brief Generate a vote
   * @param blockhash vote on PBFT block hash
   * @param type vote type
   * @param period PBFT period
   * @param round PBFT round
   * @param step PBFT step
   * @return vote
   */
  std::shared_ptr<Vote> generateVote(const blk_hash_t& blockhash, PbftVoteTypes type, PbftPeriod period,
                                     PbftRound round, PbftStep step);

  /**
   * @brief Validates vote
   *
   * @param vote to be validated
   * @return <true, ""> vote validation passed, otherwise <false, "err msg">
   */
  std::pair<bool, std::string> validateVote(const std::shared_ptr<Vote>& vote) const;

  /**
   * @brief Get 2t+1. 2t+1 is 2/3 of PBFT sortition threshold and plus 1 for a specific period
   * @param pbft_period pbft period
   * @return PBFT 2T + 1 if successful, otherwise (due to non-existent data for pbft_period) empty optional
   */
  std::optional<uint64_t> getPbftTwoTPlusOne(PbftPeriod pbft_period) const;

  bool genAndValidateVrfSortition(PbftPeriod pbft_period, PbftRound pbft_round) const;

 private:
  using UniqueLock = boost::unique_lock<boost::shared_mutex>;
  using SharedLock = boost::shared_lock<boost::shared_mutex>;

  /**
   * @brief Retrieve all verified votes from DB to the verified votes map. And broadcast all next voting type votes to
   * peers if node has extended the partition steps (1000). That only happens when nodes start.
   */
  void retreieveVotes_();

  /**
   * @brief Get PBFT sortition threshold for specific period
   * @param total_dpos_votes_count total votes count
   * @param vote_type vote type
   * @return PBFT sortition threshold
   */
  uint64_t getPbftSortitionThreshold(uint64_t total_dpos_votes_count, PbftVoteTypes vote_type) const;

 private:
  const addr_t node_addr_;
  const PbftConfig& pbft_config_;
  const vrf_wrapper::vrf_sk_t vrf_sk_;
  const secret_t node_sk_;
  const dev::Public node_pub_;

  std::shared_ptr<DbStorage> db_;
  std::shared_ptr<PbftChain> pbft_chain_;
  std::shared_ptr<FinalChain> final_chain_;
  std::shared_ptr<KeyManager> key_manager_;
  std::weak_ptr<Network> network_;

  std::unique_ptr<std::thread> daemon_;

  // TODO[1907]: this will be part of VerifiedVotes class
  // <PBFT period, <PBFT round, <PBFT step, <voted value, pair<voted weight, <vote hash, vote>>>>>
  std::map<
      PbftPeriod,
      std::map<PbftRound,
               std::map<PbftStep,
                        std::unordered_map<
                            blk_hash_t, std::pair<uint64_t, std::unordered_map<vote_hash_t, std::shared_ptr<Vote>>>>>>>
      verified_votes_;
  // Current period, it should only be used under verified_votes_access_ mutex
  PbftPeriod verified_votes_last_period_;
  mutable boost::shared_mutex verified_votes_access_;

  // <PBFT period, <PBFT round, <PBFT step, <voter address, pair<vote 1, vote 2>>><>
  // For next votes we enable 2 votes per round & step, one of which must be vote for kNullBlockHash
  std::map<PbftPeriod,
           std::map<PbftRound,
                    std::unordered_map<
                        PbftStep, std::unordered_map<addr_t, std::pair<std::shared_ptr<Vote>, std::shared_ptr<Vote>>>>>>
      voters_unique_votes_;
  mutable std::shared_mutex voters_unique_votes_mutex_;
  // TODO[1907]: end of VerifiedVotes class

  // TODO[1907]: this will be part of RewardVotes class
  std::pair<blk_hash_t, PbftPeriod> reward_votes_pbft_block_;
  PbftRound reward_votes_round_;  // round, during which was the block pushed into the chain
  std::unordered_map<vote_hash_t, std::shared_ptr<Vote>> reward_votes_;
  mutable std::shared_mutex reward_votes_mutex_;
  // TODO[1907]: end of RewardVotes class

  // Cache for current 2T+1 - do not access it directly as it is not updated automatically,
  // always call getPbftTwoTPlusOne instead !!!
  mutable std::pair<PbftPeriod, uint64_t /* two_t_plus_one for period */> current_two_t_plus_one_;
  mutable std::shared_mutex current_two_t_plus_one_mutex_;

  LOG_OBJECTS_DEFINE
};

/** @}*/

}  // namespace taraxa
