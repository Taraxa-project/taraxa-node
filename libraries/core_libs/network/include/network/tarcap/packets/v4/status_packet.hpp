#pragma once

#include "common/types.hpp"
#include "network/tarcap/packets_handlers/latest/common/exceptions.hpp"

namespace taraxa::network::tarcap::v4 {

struct StatusPacket {
  StatusPacket(const dev::RLP& packet_rlp) {
    if (const auto items_count = packet_rlp.itemCount();
        items_count != kInitialStatusPacketItemsCount && items_count != kStandardStatusPacketItemsCount) {
      throw InvalidRlpItemsCountException("StatusPacket", packet_rlp.itemCount(), kStandardStatusPacketItemsCount);
    }

    auto it = packet_rlp.begin();
    if (packet_rlp.itemCount() == kInitialStatusPacketItemsCount) {
      peer_chain_id = (*it++).toInt<uint64_t>();
      peer_dag_level = (*it++).toInt<uint64_t>();
      genesis_hash = (*it++).toHash<blk_hash_t>();
      peer_pbft_chain_size = (*it++).toInt<PbftPeriod>();
      peer_syncing = (*it++).toInt();
      peer_pbft_round = (*it++).toInt<PbftRound>();
      node_major_version = (*it++).toInt<unsigned>();
      node_minor_version = (*it++).toInt<unsigned>();
      node_patch_version = (*it++).toInt<unsigned>();
      is_light_node = (*it++).toInt();
      node_history = (*it++).toInt<PbftPeriod>();
    } else {
      peer_dag_level = (*it++).toInt<uint64_t>();
      peer_pbft_chain_size = (*it++).toInt<PbftPeriod>();
      peer_syncing = (*it++).toInt();
      peer_pbft_round = (*it++).toInt<PbftRound>();
    }
  }

  bool isInitialStatusPacket() const { return peer_chain_id.has_value(); }

  uint64_t peer_dag_level;
  PbftPeriod peer_pbft_chain_size;
  bool peer_syncing;
  PbftRound peer_pbft_round;
  std::optional<uint64_t> peer_chain_id;
  std::optional<blk_hash_t> genesis_hash;
  std::optional<unsigned> node_major_version;
  std::optional<unsigned> node_minor_version;
  std::optional<unsigned> node_patch_version;
  std::optional<bool> is_light_node;
  std::optional<PbftPeriod> node_history;

  static constexpr uint16_t kInitialStatusPacketItemsCount = 11;
  static constexpr uint16_t kStandardStatusPacketItemsCount = 4;
};

}  // namespace taraxa::network::tarcap::v4
