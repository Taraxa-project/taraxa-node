#pragma once

#include <memory>

#include "config/config.hpp"
#include "final_chain/data.hpp"
#include "logger/logger.hpp"
#include "pillar_chain/pillar_block.hpp"
#include "pillar_chain/pillar_votes.hpp"

namespace taraxa {
class DbStorage;
class Network;
class KeyManager;
class VoteManager;
}  // namespace taraxa

namespace taraxa::final_chain {
class FinalChain;
}

namespace taraxa::pillar_chain {

/** @addtogroup PILLAR_CHAIN
 * @{
 */

/**
 * @brief PillarChainMgr class contains functionality related to pillar chain
 */
class PillarChainManager {
 public:
  PillarChainManager(const FicusHardforkConfig& ficusHfConfig, std::shared_ptr<DbStorage> db,
                     std::shared_ptr<final_chain::FinalChain> final_chain, std::shared_ptr<VoteManager> vote_mgr,
                     std::shared_ptr<KeyManager> key_manager, addr_t node_addr);

  /**
   * @Process Creates new pillar block
   *
   * @param block_data
   * @return pillar block in case block was created, otherwise nullptr
   */
  std::shared_ptr<PillarBlock> createPillarBlock(const std::shared_ptr<final_chain::FinalizationResult>& block_data);

  /**
   * @brief Generate and place pillar vote for provided pillar_block_hash in case the whole pillar block is present and
   * valid
   *
   * @param pillar_block_hash
   * @param node_sk
   * @param is_pbft_syncing
   * @return true if vote placed, otherwise false
   */
  bool genAndPlacePillarVote(const PillarBlock::Hash& pillar_block_hash, const secret_t& node_sk, bool is_pbft_syncing);

  /**
   * @brief Check if pillar chain is synced - node has all previous pillar blocks(+votes) and there is 2t+1
   * votes for latest pillar block. If not, request them
   *
   * @param block_num - current block number
   *
   * @return true if synced, otherwise false
   */
  bool checkPillarChainSynced(EthBlockNumber block_num) const;

  /**
   * @brief Set network as a weak pointer
   * @param network a weak pointer
   */
  void setNetwork(std::weak_ptr<Network> network);

  /**
   * @brief Checks if vote is related to saved latest_pillar_block_ and it is not already saved
   *
   * @param vote
   * @return true if relevant, otherwise false
   */
  bool isRelevantPillarVote(const std::shared_ptr<PillarVote> vote) const;

  /**
   * @brief Validates pillar vote
   *
   * @param vote
   * @return true if valid, otherwise false
   */
  bool validatePillarVote(const std::shared_ptr<PillarVote> vote) const;

  /**
   * @return true if block_hash is the latest finalized pillar block
   */
  bool isPillarBlockLatestFinalized(const PillarBlock::Hash& block_hash) const;

  /**
   * @return last finalized pillar block
   */
  std::shared_ptr<PillarBlock> getLastFinalizedPillarBlock() const;

  /**
   * @brief Add a vote to the pillar votes map
   * @param vote vote
   *
   * @return vote's weight if vote was successfully added, otherwise 0
   */
  uint64_t addVerifiedPillarVote(const std::shared_ptr<PillarVote>& vote);

  /**
   * @brief Push new finalized pillar block
   *
   * @param pillarBlockData
   * @return true if successfully pushed, otherwise false
   */
  bool pushPillarBlock(const PillarBlockData& pillarBlockData);

  /**
   * @return current pillar block
   */
  std::shared_ptr<PillarBlock> getCurrentPillarBlock() const;

  /**
   * @brief Get all pillar votes for specified pillar block
   *
   * @param period
   * @param pillar_block_hash
   *
   * @return all pillar votes for specified period and pillar block hash
   */
  std::vector<std::shared_ptr<PillarVote>> getVerifiedPillarVotes(PbftPeriod period,
                                                                  const PillarBlock::Hash pillar_block_hash) const;

  /**
   * @return true if pillar block is valid - previous pillar block hash == last finalized pillar block hash
   */
  bool isValidPillarBlock(const std::shared_ptr<PillarBlock>& pillar_block) const;

 private:
  /**
   * @brief Return a vector of validators vote counts changes between the current and previous pillar block
   *        Changes are ordered based on validators addresses
   *
   * @param current_vote_counts
   * @param previous_pillar_block_vote_counts
   * @return ordered vector of validators vote counts changes
   */
  std::vector<PillarBlock::ValidatorVoteCountChange> getOrderedValidatorsVoteCountsChanges(
      const std::vector<state_api::ValidatorVoteCount>& current_vote_counts,
      const std::vector<state_api::ValidatorVoteCount>& previous_pillar_block_vote_counts);

 private:
  // Node config
  const FicusHardforkConfig kFicusHfConfig;

  std::shared_ptr<DbStorage> db_;
  std::weak_ptr<Network> network_;
  std::shared_ptr<final_chain::FinalChain> final_chain_;
  std::shared_ptr<VoteManager> vote_mgr_;
  std::shared_ptr<KeyManager> key_manager_;

  const addr_t node_addr_;

  // Last finalized pillar block - saved into db together with 2t+1 votes
  std::shared_ptr<PillarBlock> last_finalized_pillar_block_;
  // Current pillar block
  std::shared_ptr<PillarBlock> current_pillar_block_;
  // Full list of validators vote counts for tbe current pillar block period - no concurrent access protection needed
  std::vector<state_api::ValidatorVoteCount> current_pillar_block_vote_counts_;

  // Pillar votes for latest_pillar_block_.period - 1, latest_pillar_block_.period and potential +1 future pillar
  // block period
  PillarVotes pillar_votes_;

  // Protects latest_pillar_block_
  mutable std::shared_mutex mutex_;

  LOG_OBJECTS_DEFINE
};

/** @}*/

}  // namespace taraxa::pillar_chain
