#pragma once

#include "common/encoding_rlp.hpp"
#include "pillar_chain/bls_signature.hpp"
#include "pillar_chain/pillar_block.hpp"

namespace taraxa::pillar_chain {

struct PillarBlockData {
  std::shared_ptr<PillarBlock> block;
  std::vector<std::shared_ptr<pillar_chain::BlsSignature>> signatures;

  HAS_RLP_FIELDS
};

RLP_FIELDS_DEFINE(PillarBlockData, block, signatures)

}  // namespace taraxa::pillar_chain
