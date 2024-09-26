#pragma once

#include "dag/dag_block.hpp"
#include "transaction/system_transaction.hpp"

namespace taraxa::network {

// TODO: create new version of this packet without manual parsing
struct GetDagSyncPacket {
  GetDagSyncPacket(const dev::RLP& packet_rlp) {
    if (constexpr size_t required_size = 2; packet_rlp.itemCount() != required_size) {
      throw InvalidRlpItemsCountException("GetDagSyncPacket", packet_rlp.itemCount(), required_size);
    }

    auto it = packet_rlp.begin();
    peer_period = (*it++).toInt<PbftPeriod>();

    for (const auto block_hash_rlp : *it) {
      blocks_hashes.emplace(block_hash_rlp.toHash<blk_hash_t>());
    }
  };

  PbftPeriod peer_period;
  std::unordered_set<blk_hash_t> blocks_hashes;

  // RLP_FIELDS_DEFINE_INPLACE(transactions, dag_block)
};

}  // namespace taraxa::network
