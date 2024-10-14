#pragma once

#include "vote/pbft_vote.hpp"

namespace taraxa::network::tarcap {

struct VotesBundlePacket {
  VotesBundlePacket() = default;
  VotesBundlePacket(const VotesBundlePacket&) = default;
  VotesBundlePacket(VotesBundlePacket&&) = default;
  VotesBundlePacket& operator=(const VotesBundlePacket&) = default;
  VotesBundlePacket& operator=(VotesBundlePacket&&) = default;

  VotesBundlePacket(const dev::RLP& packet_rlp) {
    *this = util::rlp_dec<VotesBundlePacket>(packet_rlp);
    if (votes.size() == 0 || votes.size() > kMaxVotesInBundleRlp) {
      throw InvalidRlpItemsCountException("VotesBundlePacket", votes.size(), kMaxVotesInBundleRlp);
    }
  };
  dev::bytes encodeRlp() { return util::rlp_enc(*this); }

  blk_hash_t votes_bundle_block_hash;
  PbftPeriod votes_bundle_pbft_period;
  PbftRound votes_bundle_pbft_round;
  PbftStep votes_bundle_votes_step;

  std::vector<std::shared_ptr<PbftVote>> votes;

  constexpr static size_t kMaxVotesInBundleRlp{1000};

  RLP_FIELDS_DEFINE_INPLACE(votes_bundle_block_hash, votes_bundle_pbft_period, votes_bundle_pbft_round,
                            votes_bundle_votes_step, votes)
};

}  // namespace taraxa::network::tarcap
