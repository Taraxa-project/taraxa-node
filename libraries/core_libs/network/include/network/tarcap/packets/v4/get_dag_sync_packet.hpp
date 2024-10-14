#pragma once

#include "dag/dag_block.hpp"
#include "network/tarcap/packets_handlers/latest/common/exceptions.hpp"
#include "transaction/system_transaction.hpp"

namespace taraxa::network::tarcap::v4 {

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
};

}  // namespace taraxa::network::tarcap::v4
