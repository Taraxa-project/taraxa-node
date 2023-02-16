#include "common/vrf_wrapper.hpp"

namespace taraxa::vrf_wrapper {

std::pair<vrf_pk_t, vrf_sk_t> getVrfKeyPair() {
  vrf_sk_t sk;
  vrf_pk_t pk;
  crypto_vrf_keypair((unsigned char *)pk.data(), (unsigned char *)sk.data());
  return {pk, sk};
}

vrf_pk_t getVrfPublicKey(vrf_sk_t const &sk) {
  vrf_pk_t pk;
  crypto_vrf_sk_to_pk((unsigned char *)pk.data(), (unsigned char *)sk.data());
  return pk;
}

bool isValidVrfPublicKey(vrf_pk_t const &pk) { return crypto_vrf_is_valid_key((unsigned char *)pk.data()) == 1; }

std::optional<vrf_proof_t> getVrfProof(vrf_sk_t const &sk, bytes const &msg) {
  vrf_proof_t proof;
  // crypto_vrf_prove return 0 on success!
  if (!crypto_vrf_prove((unsigned char *)proof.data(), (const unsigned char *)sk.data(),
                        (const unsigned char *)msg.data(), msg.size())) {
    return proof;
  }
  return {};
}

std::optional<vrf_output_t> getVrfOutput(vrf_pk_t const &pk, vrf_proof_t const &proof, bytes const &msg) {
  vrf_output_t output;
  // crypto_vrf_verify return 0 on success!
  if (!crypto_vrf_verify((unsigned char *)output.data(), (const unsigned char *)pk.data(),
                         (const unsigned char *)proof.data(), (const unsigned char *)msg.data(), msg.size())) {
    return output;
  }
  return {};
}

dev::bytes VrfSortitionBase::makeVrfInput(taraxa::level_t level, const dev::h256 &period_hash) {
  dev::RLPStream s;
  s << level;
  s << period_hash;
  return s.invalidate();
}

bool VrfSortitionBase::verify(const vrf_pk_t &pk, const bytes &msg, uint16_t vote_count) const {
  if (!isValidVrfPublicKey(pk)) {
    return false;
  }
  auto res = vrf_wrapper::getVrfOutput(pk, proof_, msg);
  if (res != std::nullopt) {
    output_ = res.value();
    thresholdFromOutput(vote_count);
    return true;
  }
  return false;
}

}  // namespace taraxa::vrf_wrapper
