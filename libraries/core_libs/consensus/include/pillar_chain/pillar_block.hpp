#pragma once

#include <json/value.h>
#include <libdevcore/RLP.h>

#include <shared_mutex>

#include "common/encoding_rlp.hpp"
#include "common/types.hpp"
#include "final_chain/state_api_data.hpp"

namespace taraxa {
class PillarVote;
}

namespace taraxa::pillar_chain {

/** @addtogroup PILLAR_CHAIN
 * @{
 */

/**
 * @brief PillarBlock contains merkle root of all finalized blocks created in the last epoch
 */
class PillarBlock {
 public:
  // Validator votes count change
  struct ValidatorVoteCountChange {
    addr_t addr_;
    int32_t vote_count_change_;  // can be both positive or negative

    ValidatorVoteCountChange(addr_t addr, int32_t vote_count_change);
    ValidatorVoteCountChange(const dev::RLP& rlp);

    ValidatorVoteCountChange() = default;
    ValidatorVoteCountChange(const ValidatorVoteCountChange&) = default;
    ValidatorVoteCountChange& operator=(const ValidatorVoteCountChange&) = default;
    ValidatorVoteCountChange(ValidatorVoteCountChange&&) = default;
    ValidatorVoteCountChange& operator=(ValidatorVoteCountChange&&) = default;

    ~ValidatorVoteCountChange() = default;

    HAS_RLP_FIELDS
  };

 public:
  PillarBlock() = default;
  PillarBlock(const dev::RLP& rlp);
  PillarBlock(PbftPeriod period, h256 state_root, blk_hash_t previous_pillar_block_hash, h256 bridge_root, u256 epoch,
              std::vector<ValidatorVoteCountChange>&& validator_votes_count_changes);

  PillarBlock(const PillarBlock& pillar_block);

  /**
   * @return pillar block hash
   */
  blk_hash_t getHash() const;

  /**
   * @return pillar block pbft period
   */
  PbftPeriod getPeriod() const;

  /**
   * @return pillar block previous block hash
   */
  blk_hash_t getPreviousBlockHash() const;

  /**
   * @return validator vote counts changes
   */
  const std::vector<ValidatorVoteCountChange>& getValidatorsVoteCountsChanges() const;

  /**
   * @return state root
   */
  const h256& getStateRoot() const;

  /**
   * @return bridge root
   */
  const h256& getBridgeRoot() const;

  /**
   * @return epoch
   */
  const uint64_t& getEpoch() const;

  /**
   * @return pillar block rlp
   */
  dev::bytes getRlp() const;

  /**
   * @return json representation of Pillar block
   */
  Json::Value getJson() const;

  // this is needed for solidity encoding. So should be changed if fields are changed
  static constexpr uint8_t kFieldsSize = 5;
  static constexpr uint8_t kArrayPosAndSize = 2;
  static constexpr uint8_t kFieldsInVoteCount = 2;
  /**
   * @note Solidity encoding is used for data that are sent to smart contracts
   *
   * @return solidity encoded representation of pillar block
   */
  dev::bytes encodeSolidity() const;

  /**
   * @brief Decodes solidity encoded representation of pillar block
   *
   * @param enc
   * @return
   */
  static PillarBlock decodeSolidity(const bytes& enc);

  HAS_RLP_FIELDS

 private:
  // Pillar block pbft period
  PbftPeriod pbft_period_{0};

  // Pbft block(for period_) state root
  h256 state_root_{};

  // Hash of the previous pillar block as pillar blocks forms a chain
  blk_hash_t previous_pillar_block_hash_{0};

  // Bridge root
  h256 bridge_root_{};

  // Bridge epoch
  uint64_t epoch_{};

  // Delta change of validators votes count between current and latest pillar block
  std::vector<ValidatorVoteCountChange> validators_votes_count_changes_{};

  mutable std::optional<blk_hash_t> hash_;
  mutable std::shared_mutex hash_mutex_;
};

struct PillarBlockData {
  std::shared_ptr<PillarBlock> block_;
  std::vector<std::shared_ptr<PillarVote>> pillar_votes_;

  PillarBlockData(std::shared_ptr<PillarBlock> block, const std::vector<std::shared_ptr<PillarVote>>& pillar_votes);
  PillarBlockData(const dev::RLP& rlp);
  dev::bytes getRlp() const;
  Json::Value getJson(bool include_signatures) const;

  const static size_t kRlpItemCount = 2;
};

struct CurrentPillarBlockDataDb {
  std::shared_ptr<pillar_chain::PillarBlock> pillar_block;
  std::vector<state_api::ValidatorVoteCount> vote_counts;

  HAS_RLP_FIELDS
};

/** @}*/

}  // namespace taraxa::pillar_chain
