#pragma once

#include <memory>

#include "common/event.hpp"
#include "config/config.hpp"
#include "final_chain/data.hpp"
#include "logger/logger.hpp"
#include "pillar_chain/pillar_block.hpp"
#include "pillar_chain/pillar_votes.hpp"

namespace taraxa {
class DbStorage;
class Network;
class KeyManager;
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
 private:
  const util::EventEmitter<const PillarBlockData&> pillar_block_finalized_emitter_{};

 public:
  const decltype(pillar_block_finalized_emitter_)::Subscriber& pillar_block_finalized_ =
      pillar_block_finalized_emitter_;

 public:
  PillarChainManager(const FicusHardforkConfig& ficus_hf_config, std::shared_ptr<DbStorage> db,
                     std::shared_ptr<final_chain::FinalChain> final_chain, std::shared_ptr<KeyManager> key_manager,
                     addr_t node_addr);

  /**
   * @Process Creates new pillar block
   *
   * @param period
   * @param block_header
   * @param bridge_root
   * @param bridge_epoch
   *
   * @return pillar block in case block was created, otherwise nullptr
   */
  std::shared_ptr<PillarBlock> createPillarBlock(PbftPeriod period,
                                                 const std::shared_ptr<const final_chain::BlockHeader>& block_header,
                                                 const h256& bridge_root, const h256& bridge_epoch);

  /**
   * @brief Generate and place pillar vote for provided pillar_block_hash in case the whole pillar block is present and
   * valid
   *
   * @param period
   * @param pillar_block_hash
   * @param node_sk
   * @param broadcast_vote
   * @return vote if it was created, otherwise nullptr
   */
  std::shared_ptr<PillarVote> genAndPlacePillarVote(PbftPeriod period, const blk_hash_t& pillar_block_hash,
                                                    const secret_t& node_sk, bool broadcast_vote);

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
  bool isPillarBlockLatestFinalized(const blk_hash_t& block_hash) const;

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
   * @brief Finalize pillar block
   *
   * @param pillar_block_hash
   * @return above threshold pillar votes if successfully finalized, otherwise empty vector
   */
  std::vector<std::shared_ptr<PillarVote>> finalizePillarBlock(const blk_hash_t& pillar_block_hash);

  /**
   * @return current pillar block
   */
  std::shared_ptr<PillarBlock> getCurrentPillarBlock() const;

  /**
   * @brief Get all pillar votes for specified pillar block
   *
   * @param period
   * @param pillar_block_hash
   * @param above_threshold
   *
   * @return all pillar votes for specified period and pillar block hash. In case above_threshold == true, votes
   *         are sorted based on vote weight and the minimum number of votes above threshold are returned
   */
  std::vector<std::shared_ptr<PillarVote>> getVerifiedPillarVotes(PbftPeriod period, const blk_hash_t pillar_block_hash,
                                                                  bool above_threshold = false) const;

  /**
   * @return true if pillar block is valid - previous pillar block hash == last finalized pillar block hash
   */
  bool isValidPillarBlock(const std::shared_ptr<PillarBlock>& pillar_block) const;

  /**
   * @param period
   * @return pillar consensus threshold (total votes count / 2 + 1) for provided period
   */
  std::optional<uint64_t> getPillarConsensusThreshold(PbftPeriod period) const;

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

  /**
   * @brief Save new pillar block into db & class data members
   *
   * @param pillar_block
   * @param new_vote_counts
   */
  void saveNewPillarBlock(std::shared_ptr<PillarBlock> pillar_block,
                          std::vector<state_api::ValidatorVoteCount>&& new_vote_counts);

 private:
  // Node config
  const FicusHardforkConfig kFicusHfConfig;

  std::shared_ptr<DbStorage> db_;
  std::weak_ptr<Network> network_;
  std::shared_ptr<final_chain::FinalChain> final_chain_;
  std::shared_ptr<KeyManager> key_manager_;

  const addr_t node_addr_;

  // Last finalized pillar block - saved into db together with > threshold votes
  std::shared_ptr<PillarBlock> last_finalized_pillar_block_;
  // Current pillar block
  std::shared_ptr<PillarBlock> current_pillar_block_;
  // Full list of validators vote counts for tbe current pillar block period - no concurrent access protection needed
  std::vector<state_api::ValidatorVoteCount> current_pillar_block_vote_counts_;

  // Pillar votes for latest_pillar_block_.period - 1, latest_pillar_block_.period and potential +1 future pillar
  // block period
  PillarVotes pillar_votes_;

  // Protects last_finalized_pillar_block_ & current_pillar_block_
  mutable std::shared_mutex mutex_;

  LOG_OBJECTS_DEFINE
};

/** @}*/

}  // namespace taraxa::pillar_chain
