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
  verify();
}
bytes VdfSortition::rlp() const {
  dev::RLPStream s;
  s.appendList(6);
  s << pk;
  s << proof;
  s << vdf_msg_.level;
  s << vdf_msg_.last_pbft_hash;
  s << vdf_sol_.first;
  s << vdf_sol_.second;
  return s.out();
}
void VdfSortition::computeVdfSolution() {
  const auto msg_bytes = vrf_wrapper::getRlpBytes(vdf_msg_.toString());
  auto t1 = getCurrentTimeMilliSeconds();
  VerifierWesolowski verifier(getLambda(), getDifficulty(), msg_bytes, N);

  ProverWesolowski prover;
  vdf_sol_ = prover(verifier);  // this line takes time ...
  auto t2 = getCurrentTimeMilliSeconds();
  vdf_computation_time_ = t2 - t1;
}
bool VdfSortition::verifyVdfSolution() {
  const auto msg_bytes = vrf_wrapper::getRlpBytes(vdf_msg_.toString());
  VerifierWesolowski verifier(getLambda(), getDifficulty(), msg_bytes, N);
  return verifier(vdf_sol_);
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