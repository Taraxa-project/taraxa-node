#pragma once

#include <memory>

#include "final_chain/data.hpp"
#include "logger/logger.hpp"
#include "pillar_chain/bls_signature.hpp"
#include "pillar_chain/pillar_block.hpp"

namespace taraxa {

class DbStorage;
class Network;
class BlsSignature;
class KeyManager;
class VoteManager;

namespace final_chain {
class FinalChain;
}

/** @addtogroup PILLAR_CHAIN
 * @{
 */

/**
 * @brief PillarChainMgr class contains functionality related to pillar chain
 */
class PillarChainManager {
 public:
  PillarChainManager(std::shared_ptr<DbStorage> db, std::shared_ptr<final_chain::FinalChain> final_chain,
                     std::shared_ptr<VoteManager> vote_mgr, std::shared_ptr<KeyManager> key_manager, addr_t node_addr);

  /**
   * @Process new final block
   *
   * @param block_data
   */
  void newFinalBlock(const std::shared_ptr<final_chain::FinalizationResult>& block_data);

  /**
   * @brief Set network as a weak pointer
   * @param network a weak pointer
   */
  void setNetwork(std::weak_ptr<Network> network);

  /**
   * @brief Checks if signature is related to saved last_pillar_block_ and it is not already saved
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
   * @brief Get all bls signatures for specified pillar block
   * @return all bls signatures
   */
  std::vector<std::shared_ptr<BlsSignature>> getVerifiedBlsSignatures(const PillarBlock::Hash pillar_block_hash) const;

 private:
  std::shared_ptr<DbStorage> db_;
  std::weak_ptr<Network> network_;
  std::shared_ptr<final_chain::FinalChain> final_chain_;
  std::shared_ptr<VoteManager> vote_mgr_;
  std::shared_ptr<KeyManager> key_manager_;

  const addr_t node_addr_;

  // TODO: from wallet-config file
  std::pair<libff::alt_bn128_Fr, libff::alt_bn128_G2> bls_keys_;

  // Last processed pillar block
  // TODO: might be just atomic hash
  std::shared_ptr<PillarBlock> last_pillar_block_;

  // 2t+1 threshold for last pillar block period
  uint64_t last_pillar_block_two_t_plus_one_;

  // Last processed pillar block signatures
  std::unordered_map<BlsSignature::Hash, std::shared_ptr<BlsSignature>> last_pillar_block_signatures_;

  // Last processed pillar block signatures weight
  uint64_t last_pillar_block_signatures_weight_;

  // Protects last_pillar_block_ & last_pillar_block_signatures_ & last_pillar_block_signatures_weight_
  mutable std::shared_mutex mutex_;

  // How often is pillar block created
  // TODO: put in config
  static constexpr uint16_t kEpochBlocksNum = 100;

  // Nodes start to broadcast BLS signatures for pillar blocks with small delay to make sure that
  // all up-to-date nodes already processed the pillar block
  // TODO: validation: kBlsSigBroadcastDelayBlocks should be way smaller than kEpochBlocksNum
  static constexpr uint16_t kBlsSigBroadcastDelayBlocks = 5;

  // How often to check if node has 2t+1 bls signature for the latest pillar block & potentially trigger syncing
  // TODO: validation: kCheckLatestBlockBlsSigs should be way smaller than kEpochBlocksNum and bigger than
  // kBlsSigBroadcastDelayBlocks
  static constexpr uint16_t kCheckLatestBlockBlsSigs = 10;

  LOG_OBJECTS_DEFINE
};

/** @}*/

}  // namespace taraxa
