#include "vote/pillar_vote.hpp"

#include <libdevcore/SHA3.h>

#include "common/encoding_rlp.hpp"

namespace taraxa {

PillarVote::PillarVote(const secret_t& node_sk, PbftPeriod period, const blk_hash_t& block_hash)
    : period_(period), Vote(node_sk, block_hash) {}

PillarVote::PillarVote(const dev::RLP& rlp) {
  util::rlp_tuple(util::RLPDecoderRef(rlp, true), period_, block_hash_, vote_signature_);
  vote_hash_ = sha3(true);
}

PillarVote::PillarVote(const bytes& b) : PillarVote(dev::RLP(b)) {}

bytes PillarVote::rlp(bool inc_sig) const {
  if (inc_sig) {
    return util::rlp_enc(*this);
  }

  dev::RLPStream s(kNoSigRlpSize);
  s << period_;
  s << block_hash_;

  return s.invalidate();
}

PbftPeriod PillarVote::getPeriod() const { return period_; }

vote_hash_t PillarVote::sha3(bool inc_sig) const { return dev::sha3(rlp(inc_sig)); }

RLP_FIELDS_DEFINE(PillarVote, period_, block_hash_, vote_signature_)

}  // namespace taraxa