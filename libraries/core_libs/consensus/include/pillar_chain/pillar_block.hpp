#pragma once

#include "common/types.hpp"

namespace taraxa {

class DbStorage;

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
    addr_t addr;
    dev::s256 stake;  // can be both positive or negative

    dev::bytes getRlp() const;
  };

 public:
  PillarBlock(PbftPeriod period, h256 state_root, std::vector<ValidatorStakeChange>&& validator_stakes_changes,
              blk_hash_t previous_pillar_block_hash);

  /**
   * @return pillar block hash
   */
  Hash getHash() const;

  /**
   * @return pillar block pbft period
   */
  PbftPeriod getPeriod() const;

 private:
  /**
   * @return pillar block rlp
   */
  dev::bytes getRlp() const;

 private:
  // Pillar block pbft period
  PbftPeriod period_;

  // Pbft block(for period_) state root
  h256 state_root{};

  // Delta change of validators stakes between current and latest pillar block
  std::vector<ValidatorStakeChange> validators_stakes_changes_{};

  Hash previous_pillar_block_hash_{0};

  const Hash kCachedHash;
};

/** @}*/

}  // namespace taraxa
