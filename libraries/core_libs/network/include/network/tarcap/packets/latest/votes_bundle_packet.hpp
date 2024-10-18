#pragma once

#include "network/tarcap/packets_handlers/latest/common/exceptions.hpp"
#include "vote/pbft_vote.hpp"

namespace taraxa::network::tarcap {

struct VotesBundlePacket {
  // TODO[2870]: Create votes bundles class
  //  blk_hash_t votes_bundle_block_hash;
  //  PbftPeriod votes_bundle_pbft_period;
  //  PbftRound votes_bundle_pbft_round;
  //  PbftStep votes_bundle_votes_step;

  std::vector<std::shared_ptr<PbftVote>> votes;

  //  RLP_FIELDS_DEFINE_INPLACE(votes_bundle_block_hash, votes_bundle_pbft_period, votes_bundle_pbft_round,
  //                            votes_bundle_votes_step, votes)
  RLP_FIELDS_DEFINE_INPLACE(votes)
};

}  // namespace taraxa::network::tarcap
