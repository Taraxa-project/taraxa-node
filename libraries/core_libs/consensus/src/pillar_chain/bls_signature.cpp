#include "pillar_chain/bls_signature.hpp"
#include "common/encoding_rlp.hpp"
#include <libdevcore/SHA3.h>

namespace taraxa {

BlsSignature::BlsSignature(const dev::RLP& rlp) {
  // TODO: parse rlp
}

BlsSignature::BlsSignature(PillarBlock::Hash pillar_block_hash, const libff::alt_bn128_G2& public_key, const libff::alt_bn128_Fr& secret) {
  pillar_block_hash_ = pillar_block_hash;
  public_key_ = public_key;
  signature_ = libBLS::Bls::CoreSignAggregated( pillar_block_hash_.toString(), secret );
}

bool BlsSignature::isValid() const {
  return libBLS::Bls::CoreVerify( public_key_, pillar_block_hash_.toString(), signature_);
}

blk_hash_t BlsSignature::getPillarBlockHash() const {
  return pillar_block_hash_;
}

dev::bytes BlsSignature::getRlp() const {
  dev::RLPStream s(1);
  s << pillar_block_hash_;
  // TODO: add signature into rlp
  //s << signature_;
  //s << public_key_;

  return s.invalidate();
}

BlsSignature::Hash BlsSignature::getHash() const {
  if (!cached_hash_.has_value()) {
    cached_hash_ = dev::sha3(getRlp());
  }

  return *cached_hash_;
}

}  // namespace taraxa
