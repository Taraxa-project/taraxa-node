#pragma once

#include "common/encoding_rlp.hpp"
#include "network/tarcap/packets_handlers/latest/common/exceptions.hpp"
#include "vote/pillar_vote.hpp"
#include "vote/votes_bundle_rlp.hpp"

namespace taraxa::network::tarcap {

struct PillarVotesBundlePacket {
  std::vector<std::shared_ptr<PillarVote>> pillar_votes;

  void rlp(::taraxa::util::RLPDecoderRef encoding) { pillar_votes = decodePillarVotesBundleRlp(encoding.value); }
  void rlp(::taraxa::util::RLPEncoderRef encoding) const {
    encoding.appendRaw(encodePillarVotesBundleRlp(pillar_votes));
  }
};

}  // namespace taraxa::network::tarcap
