#include "vote/votes_bundle_rlp.hpp"

#include "vote/pbft_vote.hpp"
#include "vote/pillar_vote.hpp"

namespace taraxa {

dev::bytes encodePbftVotesBundleRlp(const std::vector<std::shared_ptr<PbftVote>>& votes) {
  if (votes.empty()) {
    assert(false);
    return {};
  }

  const auto& reference_block_hash = votes.back()->getBlockHash();
  const auto reference_period = votes.back()->getPeriod();
  const auto reference_round = votes.back()->getRound();
  const auto reference_step = votes.back()->getStep();

  dev::RLPStream votes_bundle_rlp(kPbftVotesBundleRlpSize);
  votes_bundle_rlp.append(reference_block_hash);
  votes_bundle_rlp.append(reference_period);
  votes_bundle_rlp.append(reference_round);
  votes_bundle_rlp.append(reference_step);
  votes_bundle_rlp.appendList(votes.size());

  for (const auto& vote : votes) {
    votes_bundle_rlp.appendRaw(vote->optimizedRlp());
  }

  return votes_bundle_rlp.invalidate();
}

std::vector<std::shared_ptr<PbftVote>> decodePbftVotesBundleRlp(const dev::RLP& votes_bundle_rlp) {
  assert(votes_bundle_rlp.itemCount() == kPbftVotesBundleRlpSize);

  const blk_hash_t votes_bundle_block_hash = votes_bundle_rlp[0].toHash<blk_hash_t>();
  const PbftPeriod votes_bundle_pbft_period = votes_bundle_rlp[1].toInt<PbftPeriod>();
  const PbftRound votes_bundle_pbft_round = votes_bundle_rlp[2].toInt<PbftRound>();
  const PbftStep votes_bundle_votes_step = votes_bundle_rlp[3].toInt<PbftStep>();

  std::vector<std::shared_ptr<PbftVote>> votes;
  votes.reserve(votes_bundle_rlp[4].itemCount());

  for (const auto vote_rlp : votes_bundle_rlp[4]) {
    auto vote = std::make_shared<PbftVote>(votes_bundle_block_hash, votes_bundle_pbft_period, votes_bundle_pbft_round,
                                           votes_bundle_votes_step, vote_rlp);
    votes.push_back(std::move(vote));
  }

  return votes;
}

void OptimizedPbftVotesBundle::rlp(::taraxa::util::RLPDecoderRef encoding) {
  votes = decodePbftVotesBundleRlp(encoding.value);
}
void OptimizedPbftVotesBundle::rlp(::taraxa::util::RLPEncoderRef encoding) const {
  encoding.appendRaw(encodePbftVotesBundleRlp(votes));
}

dev::bytes encodePillarVotesBundleRlp(const std::vector<std::shared_ptr<PillarVote>>& votes) {
  if (votes.empty()) {
    assert(false);
    return {};
  }

  const auto& reference_block_hash = votes.back()->getBlockHash();
  const auto reference_period = votes.back()->getPeriod();

  dev::RLPStream votes_bundle_rlp(kPillarVotesBundleRlpSize);
  votes_bundle_rlp.append(reference_block_hash);
  votes_bundle_rlp.append(reference_period);
  votes_bundle_rlp.appendList(votes.size());

  for (const auto& vote : votes) {
    votes_bundle_rlp.appendRaw(util::rlp_enc(vote->getVoteSignature()));
  }

  return votes_bundle_rlp.invalidate();
}

std::vector<std::shared_ptr<PillarVote>> decodePillarVotesBundleRlp(const dev::RLP& votes_bundle_rlp) {
  assert(votes_bundle_rlp.itemCount() == kPillarVotesBundleRlpSize);

  const blk_hash_t votes_bundle_block_hash = votes_bundle_rlp[0].toHash<blk_hash_t>();
  const PbftPeriod votes_bundle_pbft_period = votes_bundle_rlp[1].toInt<PbftPeriod>();

  std::vector<std::shared_ptr<PillarVote>> votes;
  votes.reserve(votes_bundle_rlp[2].itemCount());

  for (const auto sig_rlp : votes_bundle_rlp[2]) {
    auto vote_sig = util::rlp_dec<sig_t>(sig_rlp);
    auto vote = std::make_shared<PillarVote>(votes_bundle_pbft_period, votes_bundle_block_hash, std::move(vote_sig));
    votes.push_back(std::move(vote));
  }

  return votes;
}

void OptimizedPillarVotesBundle::rlp(::taraxa::util::RLPDecoderRef encoding) {
  pillar_votes = decodePillarVotesBundleRlp(encoding.value);
}
void OptimizedPillarVotesBundle::rlp(::taraxa::util::RLPEncoderRef encoding) const {
  encoding.appendRaw(encodePillarVotesBundleRlp(pillar_votes));
}

}  // namespace taraxa