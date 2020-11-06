#include "vdf_sortition.hpp"

#include "config.hpp"  // The order is important
#include "libdevcore/CommonData.h"

namespace taraxa::vdf_sortition {

Json::Value enc_json(VdfConfig const& obj) {
  Json::Value ret(Json::objectValue);
  ret["difficulty_selection"] = dev::toJS(obj.difficulty_selection);
  ret["difficulty_min"] = dev::toJS(obj.difficulty_min);
  ret["difficulty_max"] = dev::toJS(obj.difficulty_max);
  ret["difficulty_stale"] = dev::toJS(obj.difficulty_stale);
  ret["lambda_bound"] = dev::toJS(obj.lambda_bound);
  return ret;
}

void dec_json(Json::Value const& json, VdfConfig& obj) {
  obj.difficulty_selection = dev::jsToInt(json["difficulty_selection"].asString());
  obj.difficulty_min = dev::jsToInt(json["difficulty_min"].asString());
  obj.difficulty_max = dev::jsToInt(json["difficulty_max"].asString());
  obj.difficulty_stale = dev::jsToInt(json["difficulty_stale"].asString());
  obj.lambda_bound = dev::jsToInt(json["lambda_bound"].asString());
}

VdfSortition::VdfSortition(VdfConfig const& config, addr_t node_addr, vrf_sk_t const& sk, Message const& msg)
    : difficulty_selection_(config.difficulty_selection),
      difficulty_min_(config.difficulty_min),
      difficulty_max_(config.difficulty_max),
      difficulty_stale_(config.difficulty_stale),
      lambda_bound_(config.lambda_bound),
      msg_(msg),
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
  difficulty_selection_ = rlp[5].toInt<uint16_t>();
  difficulty_min_ = rlp[6].toInt<uint16_t>();
  difficulty_max_ = rlp[7].toInt<uint16_t>();
  difficulty_stale_ = rlp[8].toInt<uint16_t>();
  lambda_bound_ = rlp[9].toInt<uint16_t>();
}

bytes VdfSortition::rlp() const {
  dev::RLPStream s;
  s.appendList(10);
  s << pk;
  s << proof;
  s << msg_.level;
  s << vdf_sol_.first;
  s << vdf_sol_.second;
  s << difficulty_selection_;
  s << difficulty_min_;
  s << difficulty_max_;
  s << difficulty_stale_;
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

bool VdfSortition::verifyVdf(level_t propose_block_level, std::string const& vdf_input) {
  // Verify propose level
  if (getVrfMessage().level != propose_block_level) {
    LOG(log_er_) << "The proposal DAG block level is " << propose_block_level << ", but in VRF message is " << getVrfMessage().level;
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
    LOG(log_er_) << "VDF solution verification failed. VDF input " << vdf_input << ", lambda " << getLambda() << ", difficulty " << getDifficulty();
    // std::cout << *this << std::endl;
    return false;
  }

  return true;
}

uint16_t VdfSortition::getDifficulty() const {
  uint16_t difficulty;
  uint16_t t = uint16_t(output[0]);  // First byte, each byte value [0, 255]
  if (t <= difficulty_selection_) {
    difficulty = difficulty_min_ + t % (difficulty_max_ - difficulty_min_);
  } else {
    difficulty = difficulty_stale_;
  }
  return difficulty;
}

uint16_t VdfSortition::getLambda() const { return lambda_bound_; }

}  // namespace taraxa::vdf_sortition