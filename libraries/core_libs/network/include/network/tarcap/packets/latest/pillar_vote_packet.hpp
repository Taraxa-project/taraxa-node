#pragma once

#include "common/encoding_rlp.hpp"
#include "network/tarcap/packets_handlers/latest/common/exceptions.hpp"
#include "vote/pillar_vote.hpp"

namespace taraxa::network::tarcap {

struct PillarVotePacket {
  std::shared_ptr<PillarVote> pillar_vote;

  RLP_FIELDS_DEFINE_INPLACE(pillar_vote)
};

}  // namespace taraxa::network::tarcap
