#pragma once

#include "pbft/period_data.hpp"
#include "vote/pbft_vote.hpp"
#include "vote/votes_bundle_rlp.hpp"

namespace taraxa::network::tarcap {

struct PbftSyncPacket {
  bool last_block;
  PeriodData period_data;
  // TODO: should it be optional ???
  // TODO[2870]: optimize rlp size (use custom class), see encodePbftVotesBundleRlp
  std::vector<std::shared_ptr<PbftVote>> current_block_cert_votes;

  RLP_FIELDS_DEFINE_INPLACE(last_block, period_data, current_block_cert_votes)
};

}  // namespace taraxa::network::tarcap
