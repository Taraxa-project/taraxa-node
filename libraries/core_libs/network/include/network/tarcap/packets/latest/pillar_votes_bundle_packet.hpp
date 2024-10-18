#pragma once

#include "common/encoding_rlp.hpp"
#include "network/tarcap/packets_handlers/latest/common/exceptions.hpp"
#include "vote/pillar_vote.hpp"

namespace taraxa::network::tarcap {

struct PillarVotesBundlePacket {
  // TODO[2870]: optimize rlp size (use custom class), see encodePillarVotesBundleRlp
  std::vector<std::shared_ptr<PillarVote>> pillar_votes;

  RLP_FIELDS_DEFINE_INPLACE(pillar_votes)
};

}  // namespace taraxa::network::tarcap
