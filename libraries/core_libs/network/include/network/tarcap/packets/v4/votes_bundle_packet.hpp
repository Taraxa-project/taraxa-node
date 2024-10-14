#pragma once

#include "network/tarcap/packets_handlers/latest/common/exceptions.hpp"
#include "vote/pbft_vote.hpp"
#include "vote/votes_bundle_rlp.hpp"

namespace taraxa::network::tarcap::v4 {

struct VotesBundlePacket {
  VotesBundlePacket(const dev::RLP& packet_rlp) {
    auto items = packet_rlp.itemCount();
    if (items != kPbftVotesBundleRlpSize) {
      throw InvalidRlpItemsCountException("VotesBundlePacket", items, kPbftVotesBundleRlpSize);
    }

    auto votes_count = packet_rlp[kPbftVotesBundleRlpSize - 1].itemCount();
    if (votes_count == 0 || votes_count > kMaxVotesInBundleRlp) {
      throw InvalidRlpItemsCountException("VotesBundlePacket", items, kMaxVotesInBundleRlp);
    }

    votes_bundle_block_hash = packet_rlp[0].toHash<blk_hash_t>();
    votes_bundle_pbft_period = packet_rlp[1].toInt<PbftPeriod>();
    votes_bundle_pbft_round = packet_rlp[2].toInt<PbftRound>();
    votes_bundle_votes_step = packet_rlp[3].toInt<PbftStep>();

    for (const auto vote_rlp : packet_rlp[4]) {
      auto vote = std::make_shared<PbftVote>(votes_bundle_block_hash, votes_bundle_pbft_period, votes_bundle_pbft_round,
                                             votes_bundle_votes_step, vote_rlp);
      votes.emplace_back(std::move(vote));
    }
  };

  blk_hash_t votes_bundle_block_hash;
  PbftPeriod votes_bundle_pbft_period;
  PbftRound votes_bundle_pbft_round;
  PbftStep votes_bundle_votes_step;
  std::vector<std::shared_ptr<PbftVote>> votes;

  const size_t kMaxVotesInBundleRlp{1000};
};

}  // namespace taraxa::network::tarcap::v4
