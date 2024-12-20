#pragma once

#include "dag/dag_block.hpp"
#include "transaction/system_transaction.hpp"

namespace taraxa::network::tarcap {

struct GetDagSyncPacket {
  PbftPeriod peer_period;
  std::vector<blk_hash_t> blocks_hashes;

  RLP_FIELDS_DEFINE_INPLACE(peer_period, blocks_hashes)
};

}  // namespace taraxa::network::tarcap
