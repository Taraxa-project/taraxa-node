#include "vdf_sortition.hpp"
#include "libdevcore/CommonData.h"

namespace taraxa::vdf_sortition {

void VdfSortition::computeVdfSolution() {
  const auto msg_bytes = vrf_wrapper::getRlpBytes(vdf_msg.toString());
  VerifierWesolowski verifier(20, 20, msg_bytes, N);

  ProverWesolowski prover;
  vdf_sol = prover(verifier);  // this line takes time ...
  assert(verifier(vdf_sol));
}
bool VdfSortition::verifyVdfSolution() {
  const auto msg_bytes = vrf_wrapper::getRlpBytes(vdf_msg.toString());

  VerifierWesolowski verifier(20, 20, msg_bytes, N);
  return verifier(vdf_sol);
}
}  // namespace taraxa::vdf_sortition