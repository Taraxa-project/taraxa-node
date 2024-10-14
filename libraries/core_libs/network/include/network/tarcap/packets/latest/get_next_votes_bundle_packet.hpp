#pragma once

#include "common/encoding_rlp.hpp"

namespace taraxa::network::tarcap {

struct GetNextVotesBundlePacket {
  GetNextVotesBundlePacket() = default;
  GetNextVotesBundlePacket(const GetNextVotesBundlePacket&) = default;
  GetNextVotesBundlePacket(GetNextVotesBundlePacket&&) = default;
  GetNextVotesBundlePacket& operator=(const GetNextVotesBundlePacket&) = default;
  GetNextVotesBundlePacket& operator=(GetNextVotesBundlePacket&&) = default;

  GetNextVotesBundlePacket(const dev::RLP& packet_rlp) { *this = util::rlp_dec<GetNextVotesBundlePacket>(packet_rlp); };
  dev::bytes encodeRlp() { return util::rlp_enc(*this); }

  PbftPeriod peer_pbft_period;
  PbftRound peer_pbft_round;

  RLP_FIELDS_DEFINE_INPLACE(peer_pbft_period, peer_pbft_round)
};

}  // namespace taraxa::network::tarcap
