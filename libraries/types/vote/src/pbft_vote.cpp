#include "vote/pbft_vote.hpp"

#include <libdevcore/CommonJS.h>

#include "common/encoding_rlp.hpp"

namespace taraxa {

PbftVote::PbftVote(const blk_hash_t& block_hash, PbftPeriod period, PbftRound round, PbftStep step, dev::RLP const& rlp)
    : Vote(block_hash) {
  vrf_wrapper::vrf_proof_t vrf_proof;
  sig_t vote_signature;
  util::rlp_tuple(util::RLPDecoderRef(rlp, true), vrf_proof, vote_signature);

  VrfPbftSortition vrf_sortition;
  vrf_sortition.pbft_msg_.period_ = period;
  vrf_sortition.pbft_msg_.round_ = round;
  vrf_sortition.pbft_msg_.step_ = step;
  vrf_sortition.proof_ = vrf_proof;

  vrf_sortition_ = std::move(vrf_sortition);
  vote_signature_ = std::move(vote_signature);
  vote_hash_ = sha3(true);
}

PbftVote::PbftVote(const dev::RLP& rlp) {
  bytes vrf_bytes;
  if (rlp.itemCount() == 3) {
    util::rlp_tuple(util::RLPDecoderRef(rlp, true), block_hash_, vrf_bytes, vote_signature_);
  } else {
    util::rlp_tuple(util::RLPDecoderRef(rlp, true), block_hash_, vrf_bytes, vote_signature_, weight_);
  }

  vrf_sortition_ = VrfPbftSortition(vrf_bytes);
  vote_hash_ = sha3(true);
}

PbftVote::PbftVote(const bytes& b) : PbftVote(dev::RLP(b)) {}

PbftVote::PbftVote(secret_t const& node_sk, VrfPbftSortition vrf_sortition, blk_hash_t const& block_hash)
    : Vote(block_hash), vrf_sortition_(std::move(vrf_sortition)) {
  signVote(node_sk);
}

bool PbftVote::operator==(const PbftVote& other) const { return rlp() == other.rlp(); }

bytes PbftVote::rlp(bool inc_sig, bool inc_weight) const {
  dev::RLPStream s;
  uint32_t number_of_items = 2;
  if (inc_sig) {
    number_of_items++;
  }
  if (inc_weight && weight_) {
    number_of_items++;
  }

  s.appendList(number_of_items);

  s << block_hash_;
  s << vrf_sortition_.getRlpBytes();
  if (inc_sig) {
    s << vote_signature_;
  }
  if (inc_weight && weight_) {
    s << weight_.value();
  }

  return s.invalidate();
}

bytes PbftVote::optimizedRlp() const {
  dev::RLPStream s(2);
  s << vrf_sortition_.proof_;
  s << vote_signature_;

  return s.invalidate();
}

Json::Value PbftVote::toJSON() const {
  Json::Value json(Json::objectValue);
  json["hash"] = dev::toJS(getHash());
  json["voter"] = dev::toJS(getVoterAddr());
  json["signature"] = dev::toJS(getVoteSignature());
  json["block_hash"] = dev::toJS(getBlockHash());
  if (weight_.has_value()) {
    json["weight"] = dev::toJS(*weight_);
  }

  return json;
}

bool PbftVote::verifyVrfSortition(const vrf_pk_t& pk, bool strict) const { return vrf_sortition_.verify(pk, strict); }

const VrfPbftSortition& PbftVote::getVrfSortition() const { return vrf_sortition_; }

const vrf_wrapper::vrf_proof_t& PbftVote::getSortitionProof() const { return vrf_sortition_.proof_; }

uint64_t PbftVote::calculateWeight(uint64_t stake, double dpos_total_votes_count, double threshold) const {
  assert(stake);
  weight_ = vrf_sortition_.calculateWeight(stake, dpos_total_votes_count, threshold, getVoter());
  return weight_.value();
}

std::optional<uint64_t> PbftVote::getWeight() const { return weight_; }

const vrf_wrapper::vrf_output_t& PbftVote::getCredential() const { return vrf_sortition_.output_; }

PbftVoteTypes PbftVote::getType() const { return vrf_sortition_.pbft_msg_.getType(); }

PbftPeriod PbftVote::getPeriod() const { return vrf_sortition_.pbft_msg_.period_; }

PbftRound PbftVote::getRound() const { return vrf_sortition_.pbft_msg_.round_; }

PbftStep PbftVote::getStep() const { return vrf_sortition_.pbft_msg_.step_; }

vote_hash_t PbftVote::sha3(bool inc_sig) const { return dev::sha3(rlp(inc_sig)); }

}  // namespace taraxa