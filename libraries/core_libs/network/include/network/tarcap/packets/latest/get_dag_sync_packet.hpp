#pragma once

#include "dag/dag_block.hpp"
#include "transaction/system_transaction.hpp"

namespace taraxa::network::tarcap {

struct GetDagSyncPacket {
  GetDagSyncPacket() = default;
  GetDagSyncPacket(const GetDagSyncPacket&) = default;
  GetDagSyncPacket(GetDagSyncPacket&&) = default;
  GetDagSyncPacket& operator=(const GetDagSyncPacket&) = default;
  GetDagSyncPacket& operator=(GetDagSyncPacket&&) = default;

  GetDagSyncPacket(const dev::RLP& packet_rlp) { *this = util::rlp_dec<GetDagSyncPacket>(packet_rlp); }
  dev::bytes encodeRlp() { return util::rlp_enc(*this); }

  PbftPeriod peer_period;
  std::vector<blk_hash_t> blocks_hashes;

  RLP_FIELDS_DEFINE_INPLACE(peer_period, blocks_hashes)
};

}  // namespace taraxa::network::tarcap
