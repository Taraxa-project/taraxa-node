#pragma once
#include <libdevcore/RLP.h>
#include <optional>
#include "sodium.h"
#include "types.hpp"
#include "util.hpp"

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
std::optional<vrf_output_t> getVrfOutput(vrf_pk_t const &pk,
                                         vrf_proof_t const &proof,
                                         bytes const &msg);
dev::bytes getRlpBytes(std::string const &str);

}  // namespace taraxa::vrf_wrapper
