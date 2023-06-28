#include "vote/vote.hpp"

#include <libdevcore/CommonJS.h>

#include "common/encoding_rlp.hpp"

namespace taraxa {

Vote::Vote(dev::RLP const& rlp) {
  bytes vrf_bytes;
  if (rlp.itemCount() == 3) {
    util::rlp_tuple(util::RLPDecoderRef(rlp, true), blockhash_, vrf_bytes, vote_signature_);
  } else {
    util::rlp_tuple(util::RLPDecoderRef(rlp, true), blockhash_, vrf_bytes, vote_signature_, weight_);
  }

  vrf_sortition_ = VrfPbftSortition(vrf_bytes);
  vote_hash_ = sha3(true);
}

Vote::Vote(bytes const& b) : Vote(dev::RLP(b)) {}

Vote::Vote(secret_t const& node_sk, VrfPbftSortition vrf_sortition, blk_hash_t const& blockhash)
    : blockhash_(blockhash), vrf_sortition_(std::move(vrf_sortition)) {
  vote_signature_ = dev::sign(node_sk, sha3(false));
  vote_hash_ = sha3(true);
  cached_voter_ = dev::toPublic(node_sk);
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

Json::Value Vote::toJSON() const {
  Json::Value json(Json::objectValue);
  json["hash"] = dev::toJS(getHash());
  json["voter"] = dev::toJS(getVoterAddr());
  json["signature"] = dev::toJS(getVoteSignature());
  json["block_hash"] = dev::toJS(getBlockHash());
  json["weight"] = dev::toJS(*weight_);

  return json;
}
}  // namespace taraxa