#include "pillar_chain/bls_signature.hpp"

#include <libdevcore/SHA3.h>

#include "common/encoding_rlp.hpp"

namespace taraxa {

BlsSignature::BlsSignature(const dev::RLP& rlp) {
  std::string sig_str;
  util::rlp_tuple(util::RLPDecoderRef(rlp, true), pillar_block_hash_, period_,signer_addr_, sig_str);

  std::stringstream(sig_str) >> signature_;
}

BlsSignature::BlsSignature(PillarBlock::Hash pillar_block_hash, PbftPeriod period, const addr_t& validator,
                           const libff::alt_bn128_Fr& secret)
    : pillar_block_hash_(pillar_block_hash),
      period_(period),
      signer_addr_(validator),
      signature_(libBLS::Bls::CoreSignAggregated(getHash().toString(), secret)) {}

bool BlsSignature::isValid(const std::shared_ptr<libff::alt_bn128_G2>& bls_pub_key) const {
  return libBLS::Bls::CoreVerify(*bls_pub_key, getHash().toString(), signature_);
}

blk_hash_t BlsSignature::getPillarBlockHash() const { return pillar_block_hash_; }

PbftPeriod BlsSignature::getPeriod() const {
  return period_;
}

addr_t BlsSignature::getSignerAddr() const { return signer_addr_; }

dev::bytes BlsSignature::getRlp() const {
  dev::RLPStream s(4);
  s << pillar_block_hash_;
  s << period_;
  s << signer_addr_;

  std::stringstream sig_ss;
  sig_ss << signature_;
  s << sig_ss.str();

  return s.invalidate();
}

BlsSignature::Hash BlsSignature::getHash() const {
  if (!cached_hash_.has_value()) {
    cached_hash_ = dev::sha3(getRlp());
  }

  return *cached_hash_;
}

}  // namespace taraxa
