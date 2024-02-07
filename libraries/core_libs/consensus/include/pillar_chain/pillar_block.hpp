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

  // Validator stake change
  struct ValidatorStakeChange {
    addr_t addr_;
    dev::s96 stake_change_;  // can be both positive or negative

    ValidatorStakeChange(const state_api::ValidatorStake& stake);
    ValidatorStakeChange(addr_t addr, dev::s96 stake_change);
    ValidatorStakeChange(const dev::RLP& rlp);

    ValidatorStakeChange() = default;
    ValidatorStakeChange(const ValidatorStakeChange&) = default;
    ValidatorStakeChange& operator=(const ValidatorStakeChange&) = default;
    ValidatorStakeChange(ValidatorStakeChange&&) = default;
    ValidatorStakeChange& operator=(ValidatorStakeChange&&) = default;

    ~ValidatorStakeChange() = default;

    HAS_RLP_FIELDS
  };

 public:
  PillarBlock() = default;
  PillarBlock(const dev::RLP& rlp);
  PillarBlock(PbftPeriod period, h256 state_root, h256 bridge_root, blk_hash_t previous_pillar_block_hash,
              std::vector<ValidatorStakeChange>&& validator_stakes_changes);

  PillarBlock(const PillarBlock& pillar_block);

  /**
   * @return pillar block hash
   */
  Hash getHash();

  /**
   * @return pillar block pbft period
   */
  PbftPeriod getPeriod() const;

  /**
   * @return pillar block previous block hash
   */
  Hash getPreviousBlockHash() const;

  /**
   * @return pillar block rlp
   */
  dev::bytes getRlp() const;

  dev::bytes encode() const;

  HAS_RLP_FIELDS

 private:
  // Pillar block pbft period
  PbftPeriod period_{0};

  // Pbft block(for period_) state root
  h256 state_root_{};

  // Bridge root
  h256 bridge_root_{};

  // Previous pillar block block hash
  Hash previous_pillar_block_hash_{0};

  // Delta change of validators stakes between current and latest pillar block
  std::vector<ValidatorStakeChange> validators_stakes_changes_{};

  std::optional<Hash> hash_;
  std::shared_mutex hash_mutex_;
};

struct PillarBlockData {
  std::shared_ptr<PillarBlock> block_;
  std::vector<std::shared_ptr<PillarVote>> pillar_votes_;

  PillarBlockData(std::shared_ptr<PillarBlock> block, const std::vector<std::shared_ptr<PillarVote>>& pillar_votes);
  PillarBlockData(const dev::RLP& rlp);
  dev::bytes getRlp() const;

  const static size_t kRlpItemCount = 2;
  HAS_RLP_FIELDS
};

/** @}*/

}  // namespace taraxa::pillar_chain
