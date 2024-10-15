#pragma once

namespace taraxa::network::tarcap {

// TODO: create new version of this packet without manual parsing
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

  StatusPacket() = default;
  StatusPacket(const StatusPacket&) = default;
  StatusPacket(StatusPacket&&) = default;
  StatusPacket& operator=(const StatusPacket&) = default;
  StatusPacket& operator=(StatusPacket&&) = default;
  StatusPacket(PbftPeriod peer_pbft_chain_size, PbftRound peer_pbft_round, uint64_t peer_dag_level, bool peer_syncing,
               std::optional<InitialData> initial_data = {})
      : peer_pbft_chain_size(peer_pbft_chain_size),
        peer_pbft_round(peer_pbft_round),
        peer_dag_level(peer_dag_level),
        peer_syncing(peer_syncing),
        initial_data(std::move(initial_data)) {}
  StatusPacket(const dev::RLP& packet_rlp) { *this = util::rlp_dec<StatusPacket>(packet_rlp); }
  dev::bytes encodeRlp() { return util::rlp_enc(*this); }

  PbftPeriod peer_pbft_chain_size;
  PbftRound peer_pbft_round;
  uint64_t peer_dag_level;
  bool peer_syncing;
  std::optional<InitialData> initial_data;

  RLP_FIELDS_DEFINE_INPLACE(peer_pbft_chain_size, peer_pbft_round, peer_dag_level, peer_syncing, initial_data)
};

}  // namespace taraxa::network::tarcap
