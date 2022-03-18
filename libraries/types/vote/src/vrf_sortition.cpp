
#include "vote/vrf_sortition.hpp"

#include <boost/math/distributions/binomial.hpp>

#include "common/encoding_rlp.hpp"

namespace taraxa {

VrfPbftSortition::VrfPbftSortition(bytes const& b) {
  dev::RLP const rlp(b);

  uint8_t pbft_msg_type;
  util::rlp_tuple(util::RLPDecoderRef(rlp, true), pk_, pbft_msg_type, pbft_msg_.round, pbft_msg_.step, proof_);
  pbft_msg_.type = static_cast<PbftVoteTypes>(pbft_msg_type);
}

bytes VrfPbftSortition::getRlpBytes() const {
  dev::RLPStream s;

  s.appendList(5);
  s << pk_;
  s << static_cast<uint8_t>(pbft_msg_.type);
  s << pbft_msg_.round;
  s << pbft_msg_.step;
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

uint64_t VrfPbftSortition::calculateWeight(uint64_t stake, double dpos_total_votes_count, double threshold,
                                           const public_t& address) const {
  // Also hash in the address. This is necessary to decorrelate the selection of different accounts that have the same
  // VRF key.
  HashableVrf hash_vrf(output_, address);
  return VrfPbftSortition::getBinominalDistribution(stake, dpos_total_votes_count, threshold, hash_vrf.getHash());
}

}  // namespace taraxa