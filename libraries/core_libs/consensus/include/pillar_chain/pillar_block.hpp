#pragma once

#include <libdevcore/RLP.h>

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
  using Hash = uint256_hash_t;

  // Validator votes count change
  struct ValidatorVoteCountChange {
    addr_t addr_;
    int64_t vote_count_change_;  // can be both positive or negative

    ValidatorVoteCountChange(addr_t addr, int64_t vote_count_change);
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
  PillarBlock(PbftPeriod period, h256 state_root, h256 bridge_root,
              std::vector<ValidatorVoteCountChange>&& validator_votes_count_changes,
              blk_hash_t previous_pillar_block_hash);

  PillarBlock(const PillarBlock& pillar_block);

  /**
   * @return pillar block hash
   */
  Hash getHash() const;

  /**
   * @return pillar block pbft period
   */
  PbftPeriod getPeriod() const;

  /**
   * @return pillar block previous block hash
   */
  Hash getPreviousBlockHash() const;

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
   * @return pillar block rlp
   */
  dev::bytes getRlp() const;

  /**
   * @return json representation of Pillar block
   */
  Json::Value getJson() const;

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

  // Bridge root
  h256 bridge_root_{};

  // Delta change of validators votes count between current and latest pillar block
  std::vector<ValidatorVoteCountChange> validators_votes_count_changes_{};

  Hash previous_pillar_block_hash_{0};

  mutable std::optional<Hash> hash_;
  mutable std::shared_mutex hash_mutex_;
};

struct PillarBlockData {
  std::shared_ptr<PillarBlock> block_;
  std::vector<std::shared_ptr<PillarVote>> pillar_votes_;

  PillarBlockData(std::shared_ptr<PillarBlock> block, std::vector<std::shared_ptr<PillarVote>>&& pillar_votes);
  PillarBlockData(const dev::RLP& rlp);
  dev::bytes getRlp() const;

  const static size_t kRlpItemCount = 2;
};

/** @}*/

}  // namespace taraxa::pillar_chain
