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

class VrfSortitionBase {
 public:
  VrfSortitionBase() = default;

  VrfSortitionBase(vrf_sk_t const &sk, bytes const &msg, uint16_t vote_count = 1) {
    const auto pk(vrf_wrapper::getVrfPublicKey(sk));
    assert(isValidVrfPublicKey(pk));
    proof_ = vrf_wrapper::getVrfProof(sk, msg).value();
    output_ = vrf_wrapper::getVrfOutput(pk, proof_, msg).value();
    thresholdFromOutput(vote_count);
  }

  static dev::bytes makeVrfInput(taraxa::level_t level, const dev::h256 &period_hash);

  bool verify(const vrf_pk_t &pk, const bytes &msg, uint16_t vote_count = 1) const;

  bool operator==(VrfSortitionBase const &other) const { return proof_ == other.proof_ && output_ == other.output_; }

  virtual std::ostream &print(std::ostream &strm) const {
    strm << "\n[VRF SortitionBase] " << std::endl;
    strm << "  proof: " << proof_ << std::endl;
    strm << "  output: " << output_ << std::endl;
    return strm;
  }

  friend std::ostream &operator<<(std::ostream &strm, VrfSortitionBase const &vrf_sortition) {
    return vrf_sortition.print(strm);
  }

 private:
  void thresholdFromOutput(uint16_t vote_count) const {
    threshold_ = (((uint16_t)output_[1] << 8) | output_[0]);
    if (vote_count > 1) {
      uint16_t min_threshold = threshold_;
      uint16_t threshold_candidate = threshold_;
      // Generate sequence of thresholds using simple generator with original threshold as seed
      const uint16_t a = 48271;  // Multiplier term used by C++11's minstd_rand
      for (uint16_t vote_count_counter = 1; vote_count_counter < vote_count; vote_count_counter++) {
        threshold_candidate = threshold_candidate * a;
        if (threshold_candidate < min_threshold) {
          min_threshold = threshold_candidate;
        }
      }
      threshold_ = min_threshold;
    }
  }

 public:
  vrf_proof_t proof_;
  mutable vrf_output_t output_;
  mutable uint16_t threshold_;
};

}  // namespace taraxa::vrf_wrapper
