#pragma once

#include "network/tarcap/packets_handlers/latest/common/exceptions.hpp"
#include "vote/pbft_vote.hpp"
#include "vote/votes_bundle_rlp.hpp"

namespace taraxa::network::tarcap {

struct VotesBundlePacket {
  std::vector<std::shared_ptr<PbftVote>> votes;

  void rlp(::taraxa::util::RLPDecoderRef encoding) { votes = decodePbftVotesBundleRlp(encoding.value); }
  void rlp(::taraxa::util::RLPEncoderRef encoding) const { encoding.appendRaw(encodePbftVotesBundleRlp(votes)); }
};

}  // namespace taraxa::network::tarcap
