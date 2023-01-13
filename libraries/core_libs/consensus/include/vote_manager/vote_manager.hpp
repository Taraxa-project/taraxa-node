#pragma once

#include "common/util.hpp"
#include "common/vrf_wrapper.hpp"
#include "final_chain/final_chain.hpp"
#include "key_manager/key_manager.hpp"
#include "pbft/pbft_chain.hpp"
#include "vote/vote.hpp"
#include "vote_manager/verified_votes.hpp"

namespace taraxa {

/** @addtogroup Vote
 * @{
 */

class Network;

namespace network::tarcap {
class TaraxaPeer;
}

// TODO[1907]: refactor vote manager
/**
 * @brief VoteManager class manage votes for PBFT consensus
 */
class VoteManager {
 private:
  // TODO: use std locks
  using UniqueLock = boost::unique_lock<boost::shared_mutex>;
  using SharedLock = boost::shared_lock<boost::shared_mutex>;

 public:
  VoteManager(const addr_t& node_addr, const PbftConfig& pbft_config, const secret_t& node_sk,
              const vrf_wrapper::vrf_sk_t& vrf_sk, std::shared_ptr<DbStorage> db, std::shared_ptr<PbftChain> pbft_chain,
              std::shared_ptr<FinalChain> final_chain, std::shared_ptr<KeyManager> key_manager);
  ~VoteManager() = default;
  VoteManager(const VoteManager&) = delete;
  VoteManager(VoteManager&&) = delete;
  VoteManager& operator=(const VoteManager&) = delete;
  VoteManager& operator=(VoteManager&&) = delete;

  /**
   * @brief Set network as a weak pointer
   * @param network a weak pinter
   */
  void setNetwork(std::weak_ptr<Network> network);

  /**
   * @brief Add a vote to the verified votes map
   * @param vote vote
   *
   * @return true if vote was successfully added, otherwise false
   */
  bool addVerifiedVote(const std::shared_ptr<Vote>& vote);

  /**
   * @brief Check if the vote has been in the verified votes map
   * @param vote vote
   * @return true if exist
   */
  bool voteInVerifiedMap(std::shared_ptr<Vote> const& vote) const;

  // TODO: lock_verified_votes_mutex will be removed once we get reward votes from verified_votes_
  // TODO: can be then also private function
  /**
   * @brief Inserts unique vote
   * @param vote
   * @return true if vote was successfully inserted(it was unique) or this specific vote was already inserted, otherwise
   * false
   */
  bool insertUniqueVote(const std::shared_ptr<Vote>& vote, bool lock_verified_votes_mutex = true);

  /**
   * @param vote
   * @return <true, ""> if vote is unique per round & step & voter, otherwise <false, "err msg">
   */
  std::pair<bool, std::string> isUniqueVote(const std::shared_ptr<Vote>& vote) const;

  /**
   * @brief Get all verified votes
   * @return all verified votes
   */
  // TODO: remove - used only in tests
  std::vector<std::shared_ptr<Vote>> getVerifiedVotes() const;

  /**
   * @brief Get the total size of all verified votes
   * @return the total size of all verified votes
   */
  // TODO: remove - used only in tests
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
   * @brief Get all verified votes in proposal vote type for the current PBFT round
   * @param period new PBFT period (period == chain_size + 1)
   * @param round current PBFT round
   * @return all verified votes in proposal vote type for the current PBFT round
   */
  std::vector<std::shared_ptr<Vote>> getProposalVotes(PbftPeriod period, PbftRound round) const;

  /**
   * @brief Check if there are enough next voting type votes to set PBFT to a forward round within period
   * @param current_pbft_period current pbft period
   * @param current_pbft_round current pbft round
   * @return optional<new round> if there is enough next votes from prior round, otherwise returns empty optional
   */
  std::optional<PbftRound> determineNewRound(PbftPeriod current_pbft_period, PbftRound current_pbft_round);

  // reward votes
  // TODO: lock_verified_votes_mutex will be removed once we get reward votes from verified_votes_
  /**
   * @brief Add last period cert vote to reward_votes_ after the cert vote voted block finalized
   * @param cert vote voted to last period PBFT block
   *
   * @return true if vote was successfully added, otherwise false
   */
  bool addRewardVote(const std::shared_ptr<Vote>& vote, bool lock_verified_votes_mutex = true);

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

  /**
   * @param vote_hash
   * @return true if vote_hash was already validated, otherwise false
   */
  bool voteAlreadyValidated(const vote_hash_t& vote_hash) const;

  /**
   * @brief Generates vrf sorition and calculates its weight
   * @return true if sortition weight > 0, otherwise false
   */
  bool genAndValidateVrfSortition(PbftPeriod pbft_period, PbftRound pbft_round) const;

  /**
   * @brief Get 2t+1 voted block for specific period, round and type, e.g. soft/cert/next voted block
   *
   * @param period
   * @param round
   * @param votes_type
   * @return emoty optional if no 2t+1 voted block was found, otherwise initialized optional with block hash
   */
  std::optional<blk_hash_t> getTwoTPlusOneVotedBlock(PbftPeriod period, PbftRound round,
                                                     TwoTPlusOneVotedBlockType type) const;

  /**
   * Get 2t+1 voted block votes for specific period, round and type, e.g. soft/cert/next voted block
   *
   * @param period
   * @param round
   * @param type
   * @param peer_filter if specified, get only votes that are unknown for peer
   * @return vector of votes if 2t+1 voted block votes found, otherwise empty vector
   */
  std::vector<std::shared_ptr<Vote>> getTwoTPlusOneVotedBlockVotes(
      PbftPeriod period, PbftRound round, TwoTPlusOneVotedBlockType type,
      const std::shared_ptr<network::tarcap::TaraxaPeer>& peer_filter = {}) const;

  /**
   * Get all 2t+1 voted block next votes(both for null block as well as specific block) for specific period and round
   *
   * @param period
   * @param round
   * @param peer_filter if specified, get only votes that are unknown for peer
   * @return vector of next votes if 2t+1 voted block votes found, otherwise empty vector
   */
  std::vector<std::shared_ptr<Vote>> getAllTwoTPlusOneNextVotes(
      PbftPeriod period, PbftRound round, const std::shared_ptr<network::tarcap::TaraxaPeer>& peer_filter = {}) const;

  /**
   * @brief Sets current pbft period & round. It also checks if we dont alredy have 2t+1 vote bundles(pf any type) for
   *                the provided period & round and if so, it saves these bundles into db
   *
   * @param pbft_period
   * @param pbft_round
   */
  void setCurrentPbftPeriodAndRound(PbftPeriod pbft_period, PbftRound pbft_round);

 private:
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
  const addr_t kNodeAddr;
  const PbftConfig& kPbftConfig;
  const vrf_wrapper::vrf_sk_t kVrfSk;
  const secret_t kNodeSk;
  const dev::Public kNodePub;

  std::shared_ptr<DbStorage> db_;
  std::shared_ptr<PbftChain> pbft_chain_;
  std::shared_ptr<FinalChain> final_chain_;
  std::shared_ptr<KeyManager> key_manager_;
  std::weak_ptr<Network> network_;

  // Current pbft period based on pbft_manager
  std::atomic<PbftPeriod> current_pbft_period_{0};
  // Current pbft round based on pbft_manager
  std::atomic<PbftRound> current_pbft_round_{0};

  // Main storage for all verified votes
  std::map<PbftPeriod, std::map<PbftRound, VerifiedVotes>> verified_votes_;
  mutable boost::shared_mutex verified_votes_access_;

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

  // Votes that have been already validated in terms of signature, stake, etc...
  // It is used as protection against ddos attack so we do no validate/process vote more than once
  mutable ExpirationCache<vote_hash_t> already_validated_votes_;

  LOG_OBJECTS_DEFINE
};

/** @}*/

}  // namespace taraxa
