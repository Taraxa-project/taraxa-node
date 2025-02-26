#pragma once

#include "common/encoding_rlp.hpp"
#include "common/types.hpp"

namespace taraxa::network::tarcap::v4 {

struct StatusPacket {
  struct InitialData {
    uint64_t peer_chain_id;
    blk_hash_t genesis_hash;
    unsigned node_major_version;
    unsigned node_minor_version;
    unsigned node_patch_version;
    bool is_light_node;
    PbftPeriod node_history;

    RLP_FIELDS_DEFINE_INPLACE(peer_chain_id, genesis_hash, node_major_version, node_minor_version, node_patch_version,
                              is_light_node, node_history)
  };

  PbftPeriod peer_pbft_chain_size;
  PbftRound peer_pbft_round;
  uint64_t peer_dag_level;
  bool peer_syncing;
  std::optional<InitialData> initial_data;

  RLP_FIELDS_DEFINE_INPLACE(peer_pbft_chain_size, peer_pbft_round, peer_dag_level, peer_syncing, initial_data)
};

}  // namespace taraxa::network::tarcap::v4
