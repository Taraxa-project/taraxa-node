#include "pillar_chain/bls_signature.hpp"

#include <libdevcore/SHA3.h>

#include "common/encoding_rlp.hpp"

namespace taraxa {

BlsSignature::BlsSignature(const dev::RLP& rlp) {
  std::string sig_str, pk_str;
  util::rlp_tuple(util::RLPDecoderRef(rlp, true), pillar_block_hash_, sig_str, pk_str);

  std::stringstream(sig_str) >> signature_;
  std::stringstream(pk_str) >> public_key_;
}

BlsSignature::BlsSignature(PillarBlock::Hash pillar_block_hash, const libff::alt_bn128_G2& public_key,
                           const libff::alt_bn128_Fr& secret)
    : pillar_block_hash_(pillar_block_hash),
      signature_(libBLS::Bls::CoreSignAggregated(getHash().toString(), secret)),
      public_key_(public_key) {}

bool BlsSignature::isValid() const { return libBLS::Bls::CoreVerify(public_key_, getHash().toString(), signature_); }

blk_hash_t BlsSignature::getPillarBlockHash() const { return pillar_block_hash_; }

dev::bytes BlsSignature::getRlp() const {
  dev::RLPStream s(3);
  s << pillar_block_hash_;

  std::stringstream sig_ss;
  sig_ss << signature_;
  s << sig_ss.str();

  std::stringstream pk_ss;
  pk_ss << public_key_;
  s << pk_ss.str();

  return s.invalidate();
}

BlsSignature::Hash BlsSignature::getHash() const {
  if (!cached_hash_.has_value()) {
    cached_hash_ = dev::sha3(getRlp());
  }

  return *cached_hash_;
}

}  // namespace taraxa
