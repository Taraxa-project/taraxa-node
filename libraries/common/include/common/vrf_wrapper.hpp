#pragma once

#include <libdevcore/RLP.h>

#include <optional>

#include "common/types.hpp"
#include "common/util.hpp"
#include "sodium.h"

namespace taraxa::vrf_wrapper {

using dev::bytes;
using vrf_pk_t = dev::FixedHash<crypto_vrf_PUBLICKEYBYTES>;   // 32
using vrf_sk_t = dev::FixedHash<crypto_vrf_SECRETKEYBYTES>;   // 64
using vrf_proof_t = dev::FixedHash<crypto_vrf_PROOFBYTES>;    // 80
using vrf_output_t = dev::FixedHash<crypto_vrf_OUTPUTBYTES>;  // 64

std::pair<vrf_pk_t, vrf_sk_t> getVrfKeyPair();
vrf_pk_t getVrfPublicKey(vrf_sk_t const &sk);
bool isValidVrfPublicKey(vrf_pk_t const &pk);
// get proof if public is valid
std::optional<vrf_proof_t> getVrfProof(vrf_sk_t const &pk, bytes const &msg);
// get output if proff is valid
std::optional<vrf_output_t> getVrfOutput(vrf_pk_t const &pk, vrf_proof_t const &proof, bytes const &msg);

struct VrfSortitionBase {
  VrfSortitionBase() = default;
  VrfSortitionBase(vrf_sk_t const &sk, bytes const &msg) : pk(vrf_wrapper::getVrfPublicKey(sk)) {
    assert(isValidVrfPublicKey(pk));
    proof = vrf_wrapper::getVrfProof(sk, msg).value();
    output = vrf_wrapper::getVrfOutput(pk, proof, msg).value();
    thresholdFromOutput();
  }
  bool verify(bytes const &msg) const;
  bool operator==(VrfSortitionBase const &other) const {
    return pk == other.pk && proof == other.proof && output == other.output;
  }
  void thresholdFromOutput() const { threshold = (((uint16_t)output[1] << 8) | output[0]); }
  virtual std::ostream &print(std::ostream &strm) const {
    strm << "\n[VRF SortitionBase] " << std::endl;
    strm << "  pk: " << pk << std::endl;
    strm << "  proof: " << proof << std::endl;
    strm << "  output: " << output << std::endl;
    return strm;
  }
  friend std::ostream &operator<<(std::ostream &strm, VrfSortitionBase const &vrf_sortition) {
    return vrf_sortition.print(strm);
  }
  vrf_pk_t pk;
  vrf_proof_t proof;
  mutable vrf_output_t output;
  mutable uint16_t threshold;
};

}  // namespace taraxa::vrf_wrapper
