#include "vdf_sortition.hpp"
#include <libdevcrypto/Common.h>
#include <libethcore/Common.h>
#include "libdevcore/CommonData.h"
#include "util.hpp"
namespace taraxa::vdf_sortition {
VdfSortition::VdfSortition(bytes const& b) {
  if (b.empty()) return;
  dev::RLP const rlp(b);
  if (!rlp.isList()) {
    throw std::invalid_argument("VdfSortition RLP must be a list");
  }
  pk = rlp[0].toHash<vrf_pk_t>();
  proof = rlp[1].toHash<vrf_proof_t>();
  vdf_msg_.level = rlp[2].toInt<uint64_t>();
  vdf_msg_.last_pbft_hash = rlp[3].toHash<blk_hash_t>();
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
  s << vdf_msg_.level;
  s << vdf_msg_.last_pbft_hash;
  s << vdf_sol_.first;
  s << vdf_sol_.second;
  s << difficulty_bound_;
  s << lambda_bits_;
  return s.out();
}
void VdfSortition::computeVdfSolution() {
  assert(verifyVrf());
  const auto msg_bytes = vrf_wrapper::getRlpBytes(vdf_msg_.toString());
  auto t1 = getCurrentTimeMilliSeconds();
  VerifierWesolowski verifier(getLambda(), getDifficulty(), msg_bytes, N);

  ProverWesolowski prover;
  vdf_sol_ = prover(verifier);  // this line takes time ...
  auto t2 = getCurrentTimeMilliSeconds();
  vdf_computation_time_ = t2 - t1;
}
bool VdfSortition::verifyVdfSolution() {
  assert(verifyVrf());
  const auto msg_bytes = vrf_wrapper::getRlpBytes(vdf_msg_.toString());
  VerifierWesolowski verifier(getLambda(), getDifficulty(), msg_bytes, N);
  if (!verifier(vdf_sol_)) {
    // std::cout << "Error! Vdf verify failed..." << std::endl;
    // std::cout << *this << std::endl;
    return false;
  }
  return true;
}

int VdfSortition::getDifficulty() const {
  return int(output[0]) % difficulty_bound_;
}

unsigned long VdfSortition::getLambda() const {
  unsigned long tmp = 0;
  for (int i = 1; i < 3; ++i) {
    tmp <<= 8;
    tmp |= output[i];
  }
  tmp >>= (16 - lambda_bits_);
  return tmp;
}

}  // namespace taraxa::vdf_sortition