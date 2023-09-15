#include "pillar_chain/pillar_block.hpp"

#include <libdevcore/SHA3.h>

#include "common/encoding_rlp.hpp"

namespace taraxa {

PillarBlock::PillarBlock(blk_hash_t epoch_blocks_merkle_root, PillarBlock::Hash previous_pillar_block_hash)
    : epoch_blocks_merkle_root_(epoch_blocks_merkle_root),
      previous_pillar_block_hash_(previous_pillar_block_hash),
      kCachedHash(dev::sha3(getRlp())) {}

dev::bytes PillarBlock::getRlp() const {
  dev::RLPStream s(2);
  s << epoch_blocks_merkle_root_;
  s << previous_pillar_block_hash_;

  return s.invalidate();
}

PillarBlock::Hash PillarBlock::getHash() const { return kCachedHash; }

}  // namespace taraxa
