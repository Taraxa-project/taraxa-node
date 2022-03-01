#include "vote/vote.hpp"

#include "common/encoding_rlp.hpp"

namespace taraxa {

Vote::Vote(dev::RLP const& rlp) {
  bytes vrf_bytes;
  if (rlp.itemCount() == 3) {
    util::rlp_tuple(util::RLPDecoderRef(rlp, true), blockhash_, vrf_bytes, vote_signature_);
  } else {
    util::rlp_tuple(util::RLPDecoderRef(rlp, true), blockhash_, vrf_bytes, vote_signature_, weight_);
  }

  vrf_sortition_ = VrfPbftSortition(std::move(vrf_bytes));
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