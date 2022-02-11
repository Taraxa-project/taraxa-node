#include "vote/vote.hpp"

#include <boost/math/distributions/binomial.hpp>

namespace taraxa {

VrfPbftSortition::VrfPbftSortition(bytes const& b) {
  dev::RLP const rlp(b);
  if (!rlp.isList()) {
    throw std::invalid_argument("VrfPbftSortition RLP must be a list");
  }
  auto it = rlp.begin();

  pk = (*it++).toHash<vrf_pk_t>();
  pbft_msg.type = PbftVoteTypes((*it++).toInt<uint>());
  pbft_msg.round = (*it++).toInt<uint64_t>();
  pbft_msg.step = (*it++).toInt<size_t>();
  proof = (*it++).toHash<vrf_proof_t>();
}

bytes VrfPbftSortition::getRlpBytes() const {
  dev::RLPStream s;

  s.appendList(5);
  s << pk;
  s << static_cast<uint8_t>(pbft_msg.type);
  s << pbft_msg.round;
  s << pbft_msg.step;
  s << proof;

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
  HashableVrf hash_vrf(output, address);
  return VrfPbftSortition::getBinominalDistribution(stake, dpos_total_votes_count, threshold, hash_vrf.getHash());
}

Vote::Vote(dev::RLP const& rlp) {
  if (!rlp.isList()) throw std::invalid_argument("vote RLP must be a list");
  auto it = rlp.begin();

  blockhash_ = (*it++).toHash<blk_hash_t>();
  vrf_sortition_ = VrfPbftSortition((*it++).toBytes());
  vote_signature_ = (*it++).toHash<sig_t>();
  if (it != rlp.end()) weight_ = (*it).toInt<uint64_t>();
  vote_hash_ = sha3(true);
}

Vote::Vote(bytes const& b) : Vote(dev::RLP(b)) {}

Vote::Vote(secret_t const& node_sk, VrfPbftSortition vrf_sortition, blk_hash_t const& blockhash)
    : blockhash_(blockhash), vrf_sortition_(std::move(vrf_sortition)) {
  vote_signature_ = dev::sign(node_sk, sha3(false));
  vote_hash_ = sha3(true);
  cached_voter_ = dev::toPublic(node_sk);
}

void Vote::validate(uint64_t stake, double dpos_total_votes_count, double sortition_threshold) const {
  if (!stake) {
    // After deep syncing, node could receive votes but still behind, may don't have vote sender state in table
    // If in PBFT blocks syncing for cert votes, that's malicious blocks
    std::stringstream err;
    err << "Invalid stake " << *this;
    throw std::logic_error(err.str());
  }

  if (!verifyVrfSortition()) {
    std::stringstream err;
    err << "Invalid vrf proof. " << *this;
    throw std::logic_error(err.str());
  }

  if (!verifyVote()) {
    std::stringstream err;
    err << "Invalid vote signature. " << dev::toHex(rlp(false)) << "  " << *this;
    throw std::logic_error(err.str());
  }

  if (!calculateWeight(stake, dpos_total_votes_count, sortition_threshold)) {
    std::stringstream err;
    err << "Vote sortition failed. Sortition threshold " << sortition_threshold << ", DPOS total votes count "
        << dpos_total_votes_count << " " << *this;
    throw std::logic_error(err.str());
  }
}

bytes Vote::rlp(bool inc_sig, bool inc_weight) const {
  dev::RLPStream s;
  uint32_t number_of_items = 2;
  if (inc_sig) {
    number_of_items++;
  }
  if (inc_weight && weight_) {
    number_of_items++;
  }

  s.appendList(number_of_items);

  s << blockhash_;
  s << vrf_sortition_.getRlpBytes();
  if (inc_sig) {
    s << vote_signature_;
  }
  if (inc_weight && weight_) {
    s << weight_.value();
  }

  return s.invalidate();
}

}  // namespace taraxa