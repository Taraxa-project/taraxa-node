#pragma once

#include "common/encoding_rlp.hpp"
#include "common/types.hpp"

namespace taraxa::network::tarcap {

struct GetPillarVotesBundlePacket {
  PbftPeriod period;
  blk_hash_t pillar_block_hash;

  RLP_FIELDS_DEFINE_INPLACE(period, pillar_block_hash)
};

}  // namespace taraxa::network::tarcap
