#include "vote/votes_bundle.hpp"

namespace taraxa {

// TODO: maybe && votes ???
VotesBundle::VotesBundle(const std::vector<std::shared_ptr<Vote>>& votes, bool validate_common_data) : votes_(votes) {
  if (votes_.empty()) {
    throw std::runtime_error("Cannot initialize VotesBundle without any votes");
  }
  const auto& reference_vote = votes_.back();
  block_hash_ = reference_vote->getBlockHash();
  period_ = reference_vote->getPeriod();
  round_ = reference_vote->getRound();
  step_ = reference_vote->getStep();

  if (validate_common_data) {
    for (const auto& vote : votes_) {
      if (vote->getBlockHash() != block_hash_) {
        throw std::runtime_error("VotesBundle: Wrong block hash");
      }
      if (vote->getPeriod() != period_) {
        throw std::runtime_error("VotesBundle: Wrong period");
      }
      if (vote->getRound() != round_) {
        throw std::runtime_error("VotesBundle: Wrong round");
      }
      if (vote->getStep() != step_) {
        throw std::runtime_error("VotesBundle: step");
      }
    }
  }
}

// bytes VotesBundle::rlp() const {
//   // Pseudo code
//   dev::RLPStream s(5);
//
//   s << block_hash_;
//   s << period_;
//   s << round_;
//   s << step_;
//
//   s.appendList(votes_.size());
//   for (const auto& vote : votes_) {
//     // Get optimizes vote rlp with only signature & and proof
//     // s << optimized_vote_rlp
//   }
//
//   return s.invalidate();
// }

}  // namespace taraxa