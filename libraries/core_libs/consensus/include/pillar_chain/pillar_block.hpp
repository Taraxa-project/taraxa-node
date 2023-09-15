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

 public:
  // TODO: use real merkle root
  PillarBlock(blk_hash_t epoch_blocks_merkle_root, blk_hash_t previous_pillar_block_hash);

  /**
   * @return pillar block hash
   */
  Hash getHash() const;

 private:
  /**
   * @return pillar block rlp
   */
  dev::bytes getRlp() const;

 private:
  // TODO: replace with actual merkle root
  blk_hash_t epoch_blocks_merkle_root_{0};

  Hash previous_pillar_block_hash_{0};

  const Hash kCachedHash;
};

/** @}*/

}  // namespace taraxa
