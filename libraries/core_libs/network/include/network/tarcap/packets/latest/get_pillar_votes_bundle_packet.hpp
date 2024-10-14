#pragma once

#include "common/encoding_rlp.hpp"

namespace taraxa::network::tarcap {

struct GetPillarVotesBundlePacket {
  GetPillarVotesBundlePacket() = default;
  GetPillarVotesBundlePacket(const GetPillarVotesBundlePacket&) = default;
  GetPillarVotesBundlePacket(GetPillarVotesBundlePacket&&) = default;
  GetPillarVotesBundlePacket& operator=(const GetPillarVotesBundlePacket&) = default;
  GetPillarVotesBundlePacket& operator=(GetPillarVotesBundlePacket&&) = default;
  GetPillarVotesBundlePacket(PbftPeriod period, blk_hash_t pillar_block_hash)
      : period(period), pillar_block_hash(pillar_block_hash) {}
  GetPillarVotesBundlePacket(const dev::RLP& packet_rlp) {
    *this = util::rlp_dec<GetPillarVotesBundlePacket>(packet_rlp);
  }

  dev::bytes encodeRlp() { return util::rlp_enc(*this); }

  PbftPeriod period;
  blk_hash_t pillar_block_hash;

  RLP_FIELDS_DEFINE_INPLACE(period, pillar_block_hash)
};

}  // namespace taraxa::network::tarcap
