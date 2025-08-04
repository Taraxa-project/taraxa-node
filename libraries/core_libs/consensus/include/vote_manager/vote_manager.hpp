#pragma once

#include "common/util.hpp"
#include "common/vrf_wrapper.hpp"
#include "final_chain/final_chain.hpp"
#include "key_manager/key_manager.hpp"
#include "pbft/pbft_chain.hpp"
#include "vote/pbft_vote.hpp"
#include "vote_manager/verified_votes.hpp"

namespace taraxa {

/** @addtogroup Vote
 * @{
 */

class Network;
class SlashingManager;
class PbftVote;
struct PbftConfig;
struct FullNodeConfig;

namespace network::tarcap {
class TaraxaPeer;
}

/**
 * @brief VoteManager class manage votes for PBFT consensus
 */
class VoteManager {
 public:
  struct PbftBlockInfo {
    blk_hash_t block_hash;
    PbftPeriod period;
    PbftRound round;
  };

 public:
  VoteManager(const FullNodeConfig& config, std::shared_ptr<DbStorage> db, std::shared_ptr<PbftChain> pbft_chain,
              std::shared_ptr<final_chain::FinalChain> final_chain, std::shared_ptr<KeyManager> key_manager,
              std::shared_ptr<SlashingManager> slashing_manager);
  ~VoteManager() = default;
  VoteManager(const VoteManager&) = delete;
  VoteManager(VoteManager&&) = delete;
  VoteManager& operator=(const VoteManager&) = delete;
  VoteManager& operator=(VoteManager&&) = delete;

  /**
   * @brief Set network as a weak pointer
   * @param network a weak pointer
   */
  void setNetwork(std::weak_ptr<Network> network);

  /**
   * @brief Add a vote to the verified votes map
   * @param vote vote
   *
   * @return true if vote was successfully added, otherwise false
   */
  bool addVerifiedVote(const std::shared_ptr<PbftVote>& vote);

  /**
   * @brief Check if the vote has been in the verified votes map
   * @param vote vote
   * @return true if exist
   */
  bool voteInVerifiedMap(std::shared_ptr<PbftVote> const& vote) const;

  /**
   * @param vote
   * @return <true, nullptr> if vote is unique per round & step & voter, otherwise <false, existing vote>
   */
  std::pair<bool, std::shared_ptr<PbftVote>> isUniqueVote(const std::shared_ptr<PbftVote>& vote) const;

  /**
   * @brief Get all verified votes
   * @return all verified votes
   */
  std::vector<std::shared_ptr<PbftVote>> getVerifiedVotes() const;

  /**
   * @brief Get the total size of all verified votes
   * @return the total size of all verified votes
   */
  uint64_t getVerifiedVotesSize() const;

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
  std::vector<std::shared_ptr<PbftVote>> getProposalVotes(PbftPeriod period, PbftRound round) const;

  /**
   * @brief Check if there are enough next voting type votes to set PBFT to a forward round within period
   * @param current_pbft_period current pbft period
   * @param current_pbft_round current pbft round
   * @return optional<new round> if there is enough next votes from prior round, otherwise returns empty optional
   */
  std::optional<PbftRound> determineNewRound(PbftPeriod current_pbft_period, PbftRound current_pbft_round);

  /**
   * @brief Replace last & second last block 2t+1 cert votes
   *
   * @param period
   * @param round
   * @param step
   * @param block_hash
   */
  void replaceTwoTPlusOneCertVotesInfo(PbftPeriod period, PbftRound round, const blk_hash_t& block_hash);

  /**
   * @brief Check reward votes for specified pbft block
   *
   * @param pbft_block
   * @param copy_votes - if set to true, votes are copied and returned, otherwise votes are just checked if present
   * @return <true, votes> - votes are empty in case copy_votes is set to false
   */
  std::pair<bool, std::vector<std::shared_ptr<PbftVote>>> checkRewardVotes(const std::shared_ptr<PbftBlock>& pbft_block,
                                                                           bool copy_votes);

  /**
   * @brief Get reward votes with the round during which was the previous block pushed
   *
   * @param current_period
   * @return vector of reward votes
   */
  std::vector<std::shared_ptr<PbftVote>> getRewardVotes(PbftPeriod current_period);

  /**
   * @brief Get last or second last block cert votes with corresponding period
   *
   * @param period
   * @return vector of cert votes
   */
  std::vector<std::shared_ptr<PbftVote>> getLatestCertVotes(PbftPeriod period);

  /**
   * @brief Get current reward votes pbft block period
   *
   * @param pbft_period
   * @return period
   */
  PbftPeriod getRewardVotesPeriod(PbftPeriod pbft_period);

  /**
   * @param pbft_period - current pbft period
   * @return reward votes period offset relative to current pbgt period.
   */
  PbftPeriod getRewardVotesPeriodOffset(PbftPeriod pbft_period) const;

  /**
   * @brief Saves own verified vote into memory and db
   *
   * @param vote
   */
  void saveOwnVerifiedVote(const std::shared_ptr<PbftVote>& vote);

  /**
   * @return all own verified votes
   */
  std::vector<std::shared_ptr<PbftVote>> getOwnVerifiedVotes();

  /**
   * @brief Clear own verified votes
   *
   * @param write_batch
   */
  void clearOwnVerifiedVotes(Batch& write_batch);

  /**
   * @brief Place a vote, save it in the verified votes queue, and gossip to peers
   * @param blockhash vote on PBFT block hash
   * @param vote_type vote type
   * @param period PBFT period
   * @param round PBFT round
   * @param step PBFT step
   * @param step PBFT step
   * @param wallet
   */
  std::shared_ptr<PbftVote> generateVoteWithWeight(const blk_hash_t& blockhash, PbftVoteTypes vote_type,
                                                   PbftPeriod period, PbftRound round, PbftStep step,
                                                   const WalletConfig& wallet);

  /**
   * @brief Generate a vote
   * @param blockhash vote on PBFT block hash
   * @param type vote type
   * @param period PBFT period
   * @param round PBFT round
   * @param step PBFT step
   * @param wallet
   * @return vote
   */
  std::shared_ptr<PbftVote> generateVote(const blk_hash_t& blockhash, PbftVoteTypes type, PbftPeriod period,
                                         PbftRound round, PbftStep step, const WalletConfig& wallet);

  /**
   * @brief Validates vote
   *
   * @param vote to be validated
   * @param strict strict validation
   * @return <true, ""> vote validation passed, otherwise <false, "err msg">
   */
  std::pair<bool, std::string> validateVote(const std::shared_ptr<PbftVote>& vote, bool strict = true) const;

  /**
   * @brief Get 2t+1. 2t+1 is 2/3 of PBFT sortition threshold and plus 1 for a specific period
   * @param pbft_period pbft period
   * @param vote_type vote type, for which we get 2t+1
   * @return PBFT 2T + 1 if successful, otherwise (due to non-existent data for pbft_period) empty optional
   */
  std::optional<uint64_t> getPbftTwoTPlusOne(PbftPeriod pbft_period, PbftVoteTypes vote_type) const;

  /**
   * @param vote_hash
   * @return true if vote_hash was already validated, otherwise false
   */
  bool voteAlreadyValidated(const vote_hash_t& vote_hash) const;

  /**
   * @brief Generates vrf sortition and calculates its weight
   * @param wallet
   * @return true if sortition weight > 0, otherwise false
   */
  bool genAndValidateVrfSortition(PbftPeriod pbft_period, PbftRound pbft_round, const WalletConfig& wallet) const;

  /**
   * @brief Get 2t+1 voted block for specific period, round and type, e.g. soft/cert/next voted block
   *
   * @param period
   * @param round
   * @param votes_type
   * @return empty optional if no 2t+1 voted block was found, otherwise initialized optional with block hash
   */
  std::optional<blk_hash_t> getTwoTPlusOneVotedBlock(PbftPeriod period, PbftRound round,
                                                     TwoTPlusOneVotedBlockType type) const;

  /**
   * Get 2t+1 voted block votes for specific period, round and type, e.g. soft/cert/next voted block
   *
   * @param period
   * @param round
   * @param type
   * @return vector of votes if 2t+1 voted block votes found, otherwise empty vector
   */
  std::vector<std::shared_ptr<PbftVote>> getTwoTPlusOneVotedBlockVotes(PbftPeriod period, PbftRound round,
                                                                       TwoTPlusOneVotedBlockType type) const;

  /**
   * @brief Sets current pbft period & round. It also checks if we dont already have 2t+1 vote bundles(pf any type) for
   *                the provided period & round and if so, it saves these bundles into db
   *
   * @param pbft_period
   * @param pbft_round
   */
  void setCurrentPbftPeriodAndRound(PbftPeriod pbft_period, PbftRound pbft_round);

  /**
   * @brief Returns greatest step (in specified period & round), for which there is at least t+1 voting power
   *        from all nodes
   * @note It is used for triggering lambda exponential backoff
   *
   * @param period
   * @param round
   * @return greatest network 2t+1 next voting step
   */
  PbftStep getNetworkTplusOneNextVotingStep(PbftPeriod period, PbftRound round) const;

 private:
  /**
   * @param vote
   * @return true if vote is valid potential reward vote
   */
  bool isPotentialRewardVote(const std::shared_ptr<PbftVote>& vote) const;

  /**
   * @brief Inserts unique vote
   * @param vote
   * @return <true, nullptr> if vote is unique per round & step & voter, otherwise <false, existing vote>
   */
  std::pair<bool, std::shared_ptr<PbftVote>> insertUniqueVote(const std::shared_ptr<PbftVote>& vote);

  /**
   * @brief Get PBFT sortition threshold for specific period
   * @param total_dpos_votes_count total votes count
   * @param vote_type vote type
   * @return PBFT sortition threshold
   */
  uint64_t getPbftSortitionThreshold(uint64_t total_dpos_votes_count, PbftVoteTypes vote_type) const;

 private:
  const FullNodeConfig& kConfig;

  std::shared_ptr<DbStorage> db_;
  std::shared_ptr<PbftChain> pbft_chain_;
  std::shared_ptr<final_chain::FinalChain> final_chain_;
  std::shared_ptr<KeyManager> key_manager_;
  std::weak_ptr<Network> network_;
  std::shared_ptr<SlashingManager> slashing_manager_;

  // Current pbft period based on pbft_manager
  std::atomic<PbftPeriod> current_pbft_period_{0};
  // Current pbft round based on pbft_manager
  std::atomic<PbftRound> current_pbft_round_{0};

  // Main storage for all verified votes
  std::map<PbftPeriod, std::map<PbftRound, VerifiedVotes>> verified_votes_;
  mutable std::shared_mutex verified_votes_access_;

  // Last & second last pbft block info based on pbft manager
  PbftBlockInfo last_pbft_block_;
  PbftBlockInfo second_last_pbft_block_;
  mutable std::shared_mutex pbft_block_info_mutex_;

  // Own votes generated during current period & round
  std::vector<std::shared_ptr<PbftVote>> own_verified_votes_;

  // Cache for current 2T+1 - <Vote type, <period, two_t_plus_one for period>>
  // !!! Important: do not access it directly as it is not updated automatically, always call getPbftTwoTPlusOne instead
  // !!!
  mutable std::unordered_map<PbftVoteTypes, std::pair<PbftPeriod, uint64_t>> current_two_t_plus_one_;
  mutable std::shared_mutex current_two_t_plus_one_mutex_;

  // Votes that have been already validated in terms of signature, stake, etc...
  // It is used as protection against ddos attack so we do no validate/process vote more than once
  mutable ExpirationCache<vote_hash_t> already_validated_votes_;

  LOG_OBJECTS_DEFINE
};

/** @}*/

}  // namespace taraxa
