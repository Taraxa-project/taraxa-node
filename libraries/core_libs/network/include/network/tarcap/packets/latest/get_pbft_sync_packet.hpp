#pragma once

#include "common/encoding_rlp.hpp"

namespace taraxa::network::tarcap {

struct GetPbftSyncPacket {
  GetPbftSyncPacket() = default;
  GetPbftSyncPacket(const GetPbftSyncPacket&) = default;
  GetPbftSyncPacket(GetPbftSyncPacket&&) = default;
  GetPbftSyncPacket& operator=(const GetPbftSyncPacket&) = default;
  GetPbftSyncPacket& operator=(GetPbftSyncPacket&&) = default;
  GetPbftSyncPacket(size_t height_to_sync) : height_to_sync(height_to_sync) {}
  GetPbftSyncPacket(const dev::RLP& packet_rlp) { *this = util::rlp_dec<GetPbftSyncPacket>(packet_rlp); }
  dev::bytes encodeRlp() { return util::rlp_enc(*this); }

  size_t height_to_sync;

  RLP_FIELDS_DEFINE_INPLACE(height_to_sync)
};

}  // namespace taraxa::network::tarcap
