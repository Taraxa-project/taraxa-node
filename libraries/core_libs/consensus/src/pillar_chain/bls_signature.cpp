#include "pillar_chain/bls_signature.hpp"

#include <libdevcore/SHA3.h>

#include "common/encoding_rlp.hpp"

namespace taraxa::pillar_chain {

BlsSignature::BlsSignature(const dev::RLP& rlp) {
  *this = util::rlp_dec<BlsSignature>(rlp);
  // TODO: try if rlp.data().toBytes() produces the same output as getRlp()
  kCachedHash = dev::sha3(getRlp());
}

BlsSignature::BlsSignature(PillarBlock::Hash pillar_block_hash, PbftPeriod period, const addr_t& validator,
                           const libff::alt_bn128_Fr& secret)
    : pillar_block_hash_(pillar_block_hash),
      period_(period),
      signer_addr_(validator),
      signature_(libBLS::Bls::CoreSignAggregated(dev::sha3(getRlp(false)).toString(), secret)),
      kCachedHash(dev::sha3(getRlp())) {}

bool BlsSignature::isValid(const std::shared_ptr<libff::alt_bn128_G2>& bls_pub_key) const {
  return libBLS::Bls::CoreVerify(*bls_pub_key, dev::sha3(getRlp(false)).toString(), signature_);
}

blk_hash_t BlsSignature::getPillarBlockHash() const { return pillar_block_hash_; }

PbftPeriod BlsSignature::getPeriod() const { return period_; }

addr_t BlsSignature::getSignerAddr() const { return signer_addr_; }

libff::alt_bn128_G1 BlsSignature::getSignature() const { return signature_; }

dev::bytes BlsSignature::getRlp(bool include_sig) const {
  if (include_sig) {
    return util::rlp_enc(*this);
  }

  // rlp without signature
  dev::RLPStream encoding;
  util::rlp_tuple(encoding, pillar_block_hash_, period_, signer_addr_);
  return encoding.invalidate();
}

// dev::bytes BlsSignature::getOptimizedRlp() const {
//   dev::RLPStream s(2);
//   s << signer_addr_;
//
//   std::stringstream sig_ss;
//   sig_ss << signature_;
//   s << sig_ss.str();
//
//   return s.invalidate();
// }

BlsSignature::Hash BlsSignature::getHash() const { return kCachedHash; }

RLP_FIELDS_DEFINE(BlsSignature, pillar_block_hash_, period_, signer_addr_, signature_)

}  // namespace taraxa::pillar_chain
