
#include "vote/vrf_sortition.hpp"

#include <boost/math/distributions/binomial.hpp>

#include "common/encoding_rlp.hpp"

namespace taraxa {

VrfPbftMsg::VrfPbftMsg(PbftVoteTypes type, PbftPeriod period, PbftRound round, PbftStep step)
    : period_(period), round_(round), step_(step) {
  // Just to make sure developers dont try to create vote type with step that does not correspond to the type
  assert(type == getType());
  assert(type != PbftVoteTypes::invalid_vote);
}

PbftVoteTypes VrfPbftMsg::getType() const {
  switch (step_) {
    case 0:
      return PbftVoteTypes::invalid_vote;
    case 1:
      return PbftVoteTypes::propose_vote;
    case 2:
      return PbftVoteTypes::soft_vote;
    case 3:
      return PbftVoteTypes::cert_vote;
    default:
      // Vote with step >= 4 is next vote
      return PbftVoteTypes::next_vote;
  }
}

std::string VrfPbftMsg::toString() const {
  return std::to_string(period_) + "_" + std::to_string(round_) + "_" + std::to_string(step_);
}

bool VrfPbftMsg::operator==(VrfPbftMsg const& other) const {
  return period_ == other.period_ && round_ == other.round_ && step_ == other.step_;
}

bytes VrfPbftMsg::getRlpBytes() const {
  dev::RLPStream s;
  s.appendList(3);
  s << period_;
  s << round_;
  s << step_;
  return s.invalidate();
}

VrfPbftSortition::VrfPbftSortition(bytes const& b) {
  dev::RLP const rlp(b);
  util::rlp_tuple(util::RLPDecoderRef(rlp, true), pbft_msg_.period_, pbft_msg_.round_, pbft_msg_.step_, proof_);
}

bytes VrfPbftSortition::getRlpBytes() const {
  dev::RLPStream s;

  s.appendList(4);
  s << pbft_msg_.period_;
  s << pbft_msg_.round_;
  s << pbft_msg_.step_;
  s << proof_;

  return s.invalidate();
}

uint64_t VrfPbftSortition::getBinominalDistribution(uint64_t stake, double dpos_total_votes_count, double threshold,
                                                    const uint256_t& hash) {
  if (!stake) return 0;  // Stake is 0

  const auto l = static_cast<uint256_t>(hash).convert_to<boost::multiprecision::mpfr_float>();
  auto division = l / kMax256bFP;
  const double ratio = division.convert_to<double>();
  boost::math::binomial_distribution<double> dist(static_cast<double>(stake), threshold / dpos_total_votes_count);

  // Binary search for find the lowest stake <= cdf
  uint64_t start = 0;
  uint64_t end = stake - 1;
  while (start + 1 < end) {
    const auto mid = start + (end - start) / 2;
    const auto target = cdf(dist, mid);
    if (ratio <= target) {
      end = mid;
    } else {
      start = mid;
    }
  }
  // Found the correct boundary
  if (ratio <= cdf(dist, start)) return start;
  if (ratio <= cdf(dist, end)) return end;
  return stake;
}

uint64_t VrfPbftSortition::calculateWeight(uint64_t stake, uint64_t dpos_total_votes_count, uint64_t threshold,
                                           const public_t& address) const {
  // Also hash in the address. This is necessary to decorrelate the selection of different accounts that have the same
  // VRF key.
  auto vrf_hash = getVoterIndexHash(output_, address);
  return VrfPbftSortition::getBinominalDistribution(stake, dpos_total_votes_count, threshold, vrf_hash);
}

dev::h256 getVoterIndexHash(const vrf_wrapper::vrf_output_t& vrf, const public_t& address, uint64_t index) {
  dev::RLPStream s;
  s.appendList(3);
  s << vrf;
  s << address;
  s << index;
  return dev::sha3(s.invalidate());
}

}  // namespace taraxa
