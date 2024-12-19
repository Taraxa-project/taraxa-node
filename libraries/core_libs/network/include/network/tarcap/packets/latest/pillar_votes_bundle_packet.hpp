#pragma once

#include "common/encoding_rlp.hpp"
#include "network/tarcap/packets_handlers/latest/common/exceptions.hpp"
#include "vote/pillar_vote.hpp"
#include "vote/votes_bundle_rlp.hpp"

namespace taraxa::network::tarcap {

struct PillarVotesBundlePacket {
  OptimizedPillarVotesBundle pillar_votes_bundle;

  RLP_FIELDS_DEFINE_INPLACE(pillar_votes_bundle)
};

}  // namespace taraxa::network::tarcap
