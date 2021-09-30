#include "vote/vote.hpp"

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
  pbft_msg.weighted_index = (*it++).toInt<size_t>();
  proof = (*it++).toHash<vrf_proof_t>();
}

bytes VrfPbftSortition::getRlpBytes() const {
  dev::RLPStream s;

  s.appendList(6);
  s << pk;
  s << static_cast<uint8_t>(pbft_msg.type);
  s << pbft_msg.round;
  s << pbft_msg.step;
  s << pbft_msg.weighted_index;
  s << proof;

  return s.out();
}

/*
 * Sortition return true:
 * CREDENTIAL(VRF output) / MAX_HASH(max512bits) <= SORTITION THRESHOLD / DPOS TOTAL VOTES COUNT
 * i.e., CREDENTIAL * DPOS TOTAL VOTES COUNT <= SORTITION THRESHOLD * MAX_HASH
 * otherwise return false
 */
bool VrfPbftSortition::canSpeak(size_t threshold, size_t dpos_total_votes_count) const {
  uint1024_t left = (uint1024_t)((uint512_t)output) * dpos_total_votes_count;
  uint1024_t right = (uint1024_t)max512bits * threshold;
  return left <= right;
}

Vote::Vote(dev::RLP const& rlp) {
  if (!rlp.isList()) throw std::invalid_argument("vote RLP must be a list");
  auto it = rlp.begin();

  blockhash_ = (*it++).toHash<blk_hash_t>();
  vrf_sortition_ = VrfPbftSortition((*it++).toBytes());
  vote_signature_ = (*it++).toHash<sig_t>();
  vote_hash_ = sha3(true);
}

Vote::Vote(bytes const& b) : Vote(dev::RLP(b)) {}

Vote::Vote(secret_t const& node_sk, VrfPbftSortition const& vrf_sortition, blk_hash_t const& blockhash)
    : blockhash_(blockhash), vrf_sortition_(vrf_sortition) {
  vote_signature_ = dev::sign(node_sk, sha3(false));
  vote_hash_ = sha3(true);
}

void Vote::validate(size_t const dpos_total_votes_count, size_t const sortition_threshold) const {
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

  if (!verifyCanSpeak(sortition_threshold, dpos_total_votes_count)) {
    std::stringstream err;
    err << "Vote sortition failed. Sortition threshold " << sortition_threshold << ", DPOS total votes count "
        << dpos_total_votes_count << *this;
    throw std::logic_error(err.str());
  }
}

bytes Vote::rlp(bool inc_sig) const {
  dev::RLPStream s;
  s.appendList(inc_sig ? 3 : 2);

  s << blockhash_;
  s << vrf_sortition_.getRlpBytes();
  if (inc_sig) {
    s << vote_signature_;
  }

  return s.out();
}

}  // namespace taraxa