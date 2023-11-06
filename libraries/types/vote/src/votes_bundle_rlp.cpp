#include "vote/votes_bundle_rlp.hpp"

#include "vote/vote.hpp"

namespace taraxa {

dev::bytes encodeVotesBundleRlp(const std::vector<std::shared_ptr<Vote>>& votes, bool validate_common_data) {
  if (votes.empty()) {
    assert(false);
    return {};
  }

  const auto& reference_block_hash = votes.back()->getBlockHash();
  const auto reference_period = votes.back()->getPeriod();
  const auto reference_round = votes.back()->getRound();
  const auto reference_step = votes.back()->getStep();

  dev::RLPStream votes_bundle_rlp(kVotesBundleRlpSize);
  votes_bundle_rlp.append(reference_block_hash);
  votes_bundle_rlp.append(reference_period);
  votes_bundle_rlp.append(reference_round);
  votes_bundle_rlp.append(reference_step);
  votes_bundle_rlp.appendList(votes.size());

  for (const auto& vote : votes) {
    if (validate_common_data &&
        (vote->getBlockHash() != reference_block_hash || vote->getPeriod() != reference_period ||
         vote->getRound() != reference_round || vote->getStep() != reference_step)) {
      assert(false);
      return {};
    }

    votes_bundle_rlp.appendRaw(vote->optimizedRlp());
  }

  return votes_bundle_rlp.invalidate();
}

std::vector<std::shared_ptr<Vote>> decodeVotesBundleRlp(const dev::RLP& votes_bundle_rlp) {
  assert(votes_bundle_rlp.itemCount() == kVotesBundleRlpSize);

  const blk_hash_t votes_bundle_block_hash = votes_bundle_rlp[0].toHash<blk_hash_t>();
  const PbftPeriod votes_bundle_pbft_period = votes_bundle_rlp[1].toInt<PbftPeriod>();
  const PbftRound votes_bundle_pbft_round = votes_bundle_rlp[2].toInt<PbftRound>();
  const PbftStep votes_bundle_votes_step = votes_bundle_rlp[3].toInt<PbftStep>();

  std::vector<std::shared_ptr<Vote>> votes;
  votes.reserve(votes_bundle_rlp[4].itemCount());

  for (const auto vote_rlp : votes_bundle_rlp[4]) {
    auto vote = std::make_shared<Vote>(votes_bundle_block_hash, votes_bundle_pbft_period, votes_bundle_pbft_round,
                                       votes_bundle_votes_step, vote_rlp);
    votes.push_back(std::move(vote));
  }

  return votes;
}

}  // namespace taraxa