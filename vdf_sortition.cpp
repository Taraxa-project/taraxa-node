#include "vdf_sortition.hpp"
#include <libdevcrypto/Common.h>
#include <libethcore/Common.h>
#include "libdevcore/CommonData.h"
#include "util.hpp"
namespace taraxa::vdf_sortition {
VdfSortition::VdfSortition(bytes const &b) {
  if (b.empty()) {
    return;
  }
  dev::RLP const rlp(b);
  if (!rlp.isList()) {
    throw std::invalid_argument("VdfSortition RLP must be a list");
  }

  pk = rlp[0].toHash<vrf_pk_t>();
  proof = rlp[1].toHash<vrf_proof_t>();
  msg_.level = rlp[2].toInt<uint64_t>();
  msg_.last_anchor_hash = rlp[3].toHash<blk_hash_t>();
  vdf_sol_.first = rlp[4].toBytes();
  vdf_sol_.second = rlp[5].toBytes();
  difficulty_bound_ = rlp[6].toInt<uint>();
  lambda_bits_ = rlp[7].toInt<uint>();
}

bytes VdfSortition::rlp() const {
  dev::RLPStream s;
  s.appendList(8);
  s << pk;
  s << proof;
  s << msg_.level;
  s << msg_.last_anchor_hash;
  s << vdf_sol_.first;
  s << vdf_sol_.second;
  s << difficulty_bound_;
  s << lambda_bits_;
  return s.out();
}

void VdfSortition::computeVdfSolution(std::string const &msg) {
//  bool verified = verifyVrf();
//  assert(verified);
  const auto msg_bytes = vrf_wrapper::getRlpBytes(msg);
  auto t1 = getCurrentTimeMilliSeconds();
  VerifierWesolowski verifier(getLambda(), getDifficulty(), msg_bytes, N);

  ProverWesolowski prover;
  vdf_sol_ = prover(verifier);  // this line takes time ...
  auto t2 = getCurrentTimeMilliSeconds();
  vdf_computation_time_ = t2 - t1;
}

bool VdfSortition::verifyVrf() {
  return VrfSortitionBase::verify(msg_);
}

bool VdfSortition::verifyVdfSolution(std::string const &vdf_input) {
  // TODO: Nodes propose a valid DAG block, peers may fail on validation since
  //  message = last anchor hash(not at same time) + proposal level.
  //  Because nodes don't finalize at the same time, there have a PBFT lambda
  //  time difference. That will cause some nodes failed validation on a valid
  //  DAG block. Use proposal block VRF message to verify for now
  // Verify VRF output
  bool verified = verifyVrf();
  assert(verified);
  // Verify VDF solution
  const auto msg_bytes = vrf_wrapper::getRlpBytes(vdf_input);
  VerifierWesolowski verifier(getLambda(), getDifficulty(), msg_bytes, N);
  if (!verifier(vdf_sol_)) {
    // std::cout << "Error! Vdf verify failed..." << std::endl;
    // std::cout << *this << std::endl;
    return false;
  }

  return true;
}

int VdfSortition::getDifficulty() const {
  // return difficulty_bound_;
  return uint(output[0]) % difficulty_bound_;
}

unsigned long VdfSortition::getLambda() const {
//  unsigned long tmp = 0;
//  for (int i = 1; i < 3; ++i) {
//    tmp <<= 8;
//    tmp |= output[i];
//  }
//  tmp >>= (16 - lambda_bits_);
//  return tmp;
  uint output_sum = 0;
  // one byte in uint max is 255, 12 bytes max 255 * 12 = 3060
  // Set lambda bound to 1500, kind of half of that
  for (auto i = 0; i < 12; i++) {
    output_sum += uint(output[i]);
  }
  return std::min(output_sum, lambda_bits_);
}

}  // namespace taraxa::vdf_sortition