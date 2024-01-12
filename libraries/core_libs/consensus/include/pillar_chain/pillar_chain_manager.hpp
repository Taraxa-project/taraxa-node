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
                     std::shared_ptr<KeyManager> key_manager, const libff::alt_bn128_Fr& bls_secret_key,
                     addr_t node_addr);

  /**
   * @Process Creates new pillar block and broadcasts bls signature
   *
   * @param block_data
   */
  void createPillarBlock(const std::shared_ptr<final_chain::FinalizationResult>& block_data);

  /**
   * @brief Check if there is 2t+1 BLS signatures for latest pillar block. If not, request them
   *
   * @param block_num - current block number
   */
  void checkTwoTPlusOneBlsSignatures(EthBlockNumber block_num) const;

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
   * @brief Checks if signature is unique per period & validator (signer)
   *
   * @param signature
   * @return true if unique, otherwise false
   */
  bool isUniqueBlsSig(const std::shared_ptr<BlsSignature> signature) const;

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
   * @brief Get all bls signatures for specified pillar block
   *
   * @param period
   * @param pillar_block_hash
   *
   * @return all bls signatures for specified period and pillar block hash
   */
  std::vector<std::shared_ptr<BlsSignature>> getVerifiedBlsSignatures(PbftPeriod period,
                                                                      const PillarBlock::Hash pillar_block_hash) const;

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

  /**
   * @brief Checks if the latest pillar block is finalized - has 2t+1 signatures
   *
   * @param new_block_period
   * @return
   */
  bool isPreviousPillarBlockFinalized(PbftPeriod new_block_period) const;

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

  // Last processed pillar block
  // TODO: might be just atomic hash
  // TODO: !!! Important to load last pillar block from db in ctor (on restart etc...) If not, pillar chain mgr will not
  // work properly
  std::shared_ptr<PillarBlock> latest_pillar_block_;
  // Full list of validators stakes during last pillar block period
  std::vector<state_api::ValidatorStake> latest_pillar_block_stakes_;

  // Bls signatures for latest_pillar_block_.period - 1, latest_pillar_block_.period and potential +1 future pillar
  // block period
  // std::map<PbftPeriod, BlsSignatures> bls_signatures_;
  Signatures signatures_;

  // Protects latest_pillar_block_ & bls_signatures
  mutable std::shared_mutex mutex_;

  LOG_OBJECTS_DEFINE
};

/** @}*/

}  // namespace taraxa::pillar_chain
