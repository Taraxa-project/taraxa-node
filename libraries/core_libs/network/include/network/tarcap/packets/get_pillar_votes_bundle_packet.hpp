#pragma once

#include "common/encoding_rlp.hpp"

namespace taraxa::network {

struct GetPillarVotesBundlePacket {
  GetPillarVotesBundlePacket() = default;
  GetPillarVotesBundlePacket(const GetPillarVotesBundlePacket&) = default;
  GetPillarVotesBundlePacket(GetPillarVotesBundlePacket&&) = default;
  GetPillarVotesBundlePacket& operator=(const GetPillarVotesBundlePacket&) = default;
  GetPillarVotesBundlePacket& operator=(GetPillarVotesBundlePacket&&) = default;

  GetPillarVotesBundlePacket(const dev::RLP& packet_rlp) {
    *this = util::rlp_dec<GetPillarVotesBundlePacket>(packet_rlp);
  }

  dev::bytes encode() { return util::rlp_enc(*this); }

  PbftPeriod period;
  blk_hash_t pillar_block_hash;

  RLP_FIELDS_DEFINE_INPLACE(period, pillar_block_hash)
};

}  // namespace taraxa::network
