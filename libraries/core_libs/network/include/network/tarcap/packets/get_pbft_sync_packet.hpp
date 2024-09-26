#pragma once

#include "common/encoding_rlp.hpp"

namespace taraxa::network {

struct GetPbftSyncPacket {
  GetPbftSyncPacket() = default;
  GetPbftSyncPacket(const GetPbftSyncPacket&) = default;
  GetPbftSyncPacket(GetPbftSyncPacket&&) = default;
  GetPbftSyncPacket& operator=(const GetPbftSyncPacket&) = default;
  GetPbftSyncPacket& operator=(GetPbftSyncPacket&&) = default;

  GetPbftSyncPacket(const dev::RLP& packet_rlp) { *this = util::rlp_dec<GetPbftSyncPacket>(packet_rlp); }
  dev::bytes encode() { return util::rlp_enc(*this); }

  size_t height_to_sync;

  RLP_FIELDS_DEFINE_INPLACE(height_to_sync)
};

}  // namespace taraxa::network
