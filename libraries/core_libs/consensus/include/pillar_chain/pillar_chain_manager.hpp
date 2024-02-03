#pragma once

#include <memory>

#include "config/config.hpp"
#include "final_chain/data.hpp"
#include "logger/logger.hpp"
#include "pillar_chain/pillar_block.hpp"
#include "pillar_chain/signatures.hpp"

namespace taraxa {
class DbStorage;
class Network;
class BlsSignature;
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
   */
  void createPillarBlock(const std::shared_ptr<final_chain::FinalizationResult>& block_data);

  /**
   * @brief Check if pillar chain is synced - node has all previous pillar blocks(+signatures) and there is 2t+1
   * signatures for latest pillar block. If not, request them
   *
   * @param block_num - current block number
   */
  void checkPillarChainSynced(EthBlockNumber block_num) const;

  /**
   * @brief Set network as a weak pointer
   * @param network a weak pointer
   */
  void setNetwork(std::weak_ptr<Network> network);

  /**
   * @brief Checks if signature is related to saved latest_pillar_block_ and it is not already saved
   *
   * @param signature
   * @return true if relevant, otherwise false
   */
  bool isRelevantBlsSig(const std::shared_ptr<BlsSignature> signature) const;

  /**
   * @brief Validates bls signature
   *
   * @param signature
   * @return true if valid, otherwise false
   */
  bool validateBlsSignature(const std::shared_ptr<BlsSignature> signature) const;

  /**
   * @brief Add a signature to the bls signatures map
   * @param signature signature
   *
   * @return true if signature was successfully added, otherwise false
   */
  bool addVerifiedBlsSig(const std::shared_ptr<BlsSignature>& signature);

  /**
   * @brief Push new finalized pillar block
   *
   * @param pillarBlockData
   * @return true if successfully pushed, otherwise false
   */
  bool pushPillarBlock(const PillarBlockData& pillarBlockData);

  /**
   * @brief Get all bls signatures for specified pillar block
   *
   * @param period
   * @param pillar_block_hash
   *
   * @return all bls signatures for specified period and pillar block hash
   */
  std::vector<std::shared_ptr<BlsSignature>> getVerifiedBlsSignatures(PbftPeriod period,
                                                                      const PillarBlock::Hash pillar_block_hash) const;

  /**
   * @return period of the latest finalized pillar block
   */
  std::optional<PbftPeriod> getLastFinalizedPillarBlockPeriod() const;

  /**
   * @return true if pillar block is valid - previous pillar block hash == last finalized pillar block hash
   */
  bool isValidPillarBlock(const std::shared_ptr<PillarBlock>& pillar_block) const;

 private:
  /**
   * @brief Return a vector of validators stakes changes between the current and previous pillar block
   *        Changes are ordered based on validators addresses
   *
   * @param current_stakes
   * @param previous_pillar_block_stakes
   * @return ordered vector of validators stakes changes
   */
  std::vector<PillarBlock::ValidatorStakeChange> getOrderedValidatorsStakesChanges(
      const std::vector<state_api::ValidatorStake>& current_stakes,
      const std::vector<state_api::ValidatorStake>& previous_pillar_block_stakes);

 private:
  // Node config
  const FicusHardforkConfig kFicusHfConfig;

  std::shared_ptr<DbStorage> db_;
  std::weak_ptr<Network> network_;
  std::shared_ptr<final_chain::FinalChain> final_chain_;
  std::shared_ptr<VoteManager> vote_mgr_;
  std::shared_ptr<KeyManager> key_manager_;

  const addr_t node_addr_;

  const libff::alt_bn128_Fr kBlsSecretKey;

  // Last finalized pillar block - saved into db together with 2t+1 signatures
  std::shared_ptr<PillarBlock> last_finalized_pillar_block_;
  // Current pillar block
  std::shared_ptr<PillarBlock> current_pillar_block_;
  // Full list of validators stakes for tbe current pillar block period - no concurrent access protection needed
  std::vector<state_api::ValidatorStake> current_pillar_block_stakes_;

  // Bls signatures for latest_pillar_block_.period - 1, latest_pillar_block_.period and potential +1 future pillar
  // block period
  Signatures signatures_;

  // Protects latest_pillar_block_
  mutable std::shared_mutex mutex_;

  LOG_OBJECTS_DEFINE
};

/** @}*/

}  // namespace taraxa::pillar_chain
