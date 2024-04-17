#include "vote/pillar_vote.hpp"

#include <libdevcore/SHA3.h>

#include "common/encoding_rlp.hpp"
#include "common/encoding_solidity.hpp"

namespace taraxa {

PillarVote::PillarVote(const secret_t& node_sk, PbftPeriod period, const blk_hash_t& block_hash)
    : Vote(block_hash), period_(period) {
  signVote(node_sk);
}

PillarVote::PillarVote(PbftPeriod period, const blk_hash_t& block_hash, sig_t&& signature)
    : Vote(block_hash), period_(period) {
  vote_signature_ = std::move(signature);
}

PillarVote::PillarVote(const dev::RLP& rlp) {
  util::rlp_tuple(util::RLPDecoderRef(rlp, true), period_, block_hash_, vote_signature_);
  vote_hash_ = sha3(true);
}

PillarVote::PillarVote(const bytes& b) : PillarVote(dev::RLP(b)) {}

bytes PillarVote::rlp() const { return util::rlp_enc(*this); }

bytes PillarVote::encodeSolidity(bool inc_sig) const {
  if (inc_sig) {
    const auto compact = dev::CompactSignatureStruct(vote_signature_);
    return util::EncodingSolidity::pack(period_, block_hash_, compact.r, compact.vs);
  }
  return util::EncodingSolidity::pack(period_, block_hash_);
}

bytes PillarVote::compactSignatureEncodeSolidity() const {
  const auto compact = dev::CompactSignatureStruct(vote_signature_);
  return util::EncodingSolidity::pack(compact.r, compact.vs);
}

PillarVote PillarVote::decodeSolidity(const bytes& enc) {
  PillarVote v;

  if (enc.size() == 2 * util::EncodingSolidity::kWordSize) {
    util::EncodingSolidity::staticUnpack(enc, v.period_, v.block_hash_);
    return v;
  }

  dev::CompactSignatureStruct cs;
  util::EncodingSolidity::staticUnpack(enc, v.period_, v.block_hash_, cs.r, cs.vs);
  v.vote_signature_ = dev::SignatureStruct(cs);
  return v;
}

dev::CompactSignatureStruct PillarVote::compactSignatureDecodeSolidity(const bytes& enc) {
  dev::CompactSignatureStruct cs;
  util::EncodingSolidity::staticUnpack(enc, cs.r, cs.vs);

  return cs;
}

PbftPeriod PillarVote::getPeriod() const { return period_; }

vote_hash_t PillarVote::sha3(bool inc_sig) const { return dev::sha3(encodeSolidity(inc_sig)); }

RLP_FIELDS_DEFINE(PillarVote, period_, block_hash_, vote_signature_)

}  // namespace taraxa