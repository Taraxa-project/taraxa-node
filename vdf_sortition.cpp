#include "vdf_sortition.hpp"

#include "config.hpp"  // The order is important
#include "libdevcore/CommonData.h"

namespace taraxa::vdf_sortition {

VdfSortition::VdfSortition(addr_t node_addr, vrf_sk_t const& sk,
                           Message const& msg, uint difficulty_bound,
                           uint lambda_bound)
    : msg_(msg),
      difficulty_bound_(difficulty_bound),
      lambda_bound_(lambda_bound),
      VrfSortitionBase(sk, msg) {
  LOG_OBJECTS_CREATE("VDF");
}

VdfSortition::VdfSortition(addr_t node_addr, bytes const& b) {
  LOG_OBJECTS_CREATE("VDF");
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
  vdf_sol_.first = rlp[3].toBytes();
  vdf_sol_.second = rlp[4].toBytes();
  difficulty_bound_ = rlp[5].toInt<uint>();
  lambda_bound_ = rlp[6].toInt<uint>();
}

bytes VdfSortition::rlp() const {
  dev::RLPStream s;
  s.appendList(7);
  s << pk;
  s << proof;
  s << msg_.level;
  s << vdf_sol_.first;
  s << vdf_sol_.second;
  s << difficulty_bound_;
  s << lambda_bound_;
  return s.out();
}

void VdfSortition::computeVdfSolution(std::string const& msg) {
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

bool VdfSortition::verifyVdf(level_t propose_block_level,
                             std::string const& vdf_input) {
  // Verify propose level
  if (getVrfMessage().level != propose_block_level) {
    LOG(log_er_) << "The proposal DAG block level is " << propose_block_level
                 << ", but in VRF message is " << getVrfMessage().level;
    return false;
  }

  if (!verifyVdfSolution(vdf_input)) {
    return false;
  }

  return true;
}

bool VdfSortition::verifyVrf() { return VrfSortitionBase::verify(msg_); }

bool VdfSortition::verifyVdfSolution(std::string const& vdf_input) {
  // Verify VRF output
  bool verified = verifyVrf();
  assert(verified);

  // Verify VDF solution
  const auto msg_bytes = vrf_wrapper::getRlpBytes(vdf_input);
  VerifierWesolowski verifier(getLambda(), getDifficulty(), msg_bytes, N);
  if (!verifier(vdf_sol_)) {
    LOG(log_er_) << "VDF solution verification failed. VDF input " << vdf_input
                 << ", lambda " << getLambda() << ", difficulty "
                 << getDifficulty();
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
  // return lambda_bound_;
  uint output_sum = 0;
  // one byte in uint max is 255, 12 bytes max 255 * 12 = 3060
  // Set lambda bound to 1500, kind of half of that
  for (auto i = 0; i < 12; i++) {
    output_sum += uint(output[i]);
  }
  return std::min(output_sum, lambda_bound_);
}

}  // namespace taraxa::vdf_sortition