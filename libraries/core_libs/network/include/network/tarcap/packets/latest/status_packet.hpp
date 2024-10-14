#pragma once

namespace taraxa::network::tarcap {

// TODO: create new version of this packet without manual parsing
struct StatusPacket {
  StatusPacket() = default;
  StatusPacket(const StatusPacket&) = default;
  StatusPacket(StatusPacket&&) = default;
  StatusPacket& operator=(const StatusPacket&) = default;
  StatusPacket& operator=(StatusPacket&&) = default;
  StatusPacket(PbftPeriod peer_pbft_chain_size, PbftRound peer_pbft_round, uint64_t peer_dag_level, bool peer_syncing,
               std::optional<uint64_t> peer_chain_id = {}, std::optional<blk_hash_t> genesis_hash = {},
               std::optional<unsigned> node_major_version = {}, std::optional<unsigned> node_minor_version = {},
               std::optional<unsigned> node_patch_version = {}, std::optional<bool> is_light_node = {},
               std::optional<PbftPeriod> node_history = {})
      : peer_pbft_chain_size(peer_pbft_chain_size),
        peer_pbft_round(peer_pbft_round),
        peer_dag_level(peer_dag_level),
        peer_syncing(peer_syncing),
        peer_chain_id(std::move(peer_chain_id)),
        genesis_hash(std::move(genesis_hash)),
        node_major_version(std::move(node_major_version)),
        node_minor_version(std::move(node_minor_version)),
        node_patch_version(std::move(node_patch_version)),
        is_light_node(std::move(is_light_node)),
        node_history(std::move(node_history)) {}
  StatusPacket(const dev::RLP& packet_rlp) { *this = util::rlp_dec<StatusPacket>(packet_rlp); }
  dev::bytes encodeRlp() { return util::rlp_enc(*this); }

  bool isInitialStatusPacket() const { return peer_chain_id.has_value(); }

  PbftPeriod peer_pbft_chain_size;
  PbftRound peer_pbft_round;
  uint64_t peer_dag_level;
  bool peer_syncing;
  std::optional<uint64_t> peer_chain_id;
  std::optional<blk_hash_t> genesis_hash;
  std::optional<unsigned> node_major_version;
  std::optional<unsigned> node_minor_version;
  std::optional<unsigned> node_patch_version;
  std::optional<bool> is_light_node;
  std::optional<PbftPeriod> node_history;

  RLP_FIELDS_DEFINE_INPLACE(peer_pbft_chain_size, peer_pbft_round, peer_dag_level, peer_syncing, peer_chain_id,
                            genesis_hash, node_major_version, node_minor_version, node_patch_version, is_light_node,
                            node_history)
};

}  // namespace taraxa::network::tarcap
