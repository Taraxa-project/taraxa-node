#pragma once

#include "common/encoding_rlp.hpp"
#include "network/tarcap/packets_handlers/latest/common/exceptions.hpp"
#include "vote/pillar_vote.hpp"

namespace taraxa::network::tarcap {

struct PillarVotePacket {
  PillarVotePacket() = default;
  PillarVotePacket(const PillarVotePacket&) = default;
  PillarVotePacket(PillarVotePacket&&) = default;
  PillarVotePacket& operator=(const PillarVotePacket&) = default;
  PillarVotePacket& operator=(PillarVotePacket&&) = default;
  PillarVotePacket(std::shared_ptr<PillarVote> pillar_vote) : pillar_vote(std::move(pillar_vote)) {}
  PillarVotePacket(const dev::RLP& packet_rlp) { *this = util::rlp_dec<PillarVotePacket>(packet_rlp); }
  dev::bytes encodeRlp() { return util::rlp_enc(*this); }

  std::shared_ptr<PillarVote> pillar_vote;

  RLP_FIELDS_DEFINE_INPLACE(pillar_vote)
};

}  // namespace taraxa::network::tarcap
