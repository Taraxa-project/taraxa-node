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

VdfSortition::VdfSortition(VdfConfig const& config, addr_t node_addr, vrf_sk_t const& sk, bytes const& msg)
    : VrfSortitionBase(sk, msg) {
  LOG_OBJECTS_CREATE("VDF");
  difficulty_ = calculateDifficulty(config);
}

uint16_t VdfSortition::calculateDifficulty(VdfConfig const& config) const {
  uint16_t difficulty;
  uint16_t t = uint16_t(output[0]);  // First byte, each byte value [0, 255]
  if (t <= config.difficulty_selection) {
    difficulty = config.difficulty_min + t % (config.difficulty_max - config.difficulty_min);
  } else {
    difficulty = config.difficulty_stale;
  }
  return difficulty;
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

  auto it = rlp.begin();
  pk = (*it++).toHash<vrf_pk_t>();
  proof = (*it++).toHash<vrf_proof_t>();
  vdf_sol_.first = (*it++).toBytes();
  vdf_sol_.second = (*it++).toBytes();
  difficulty_ = (*it++).toInt<uint16_t>();
}

VdfSortition::VdfSortition(addr_t node_addr, Json::Value const& json) {
  LOG_OBJECTS_CREATE("VDF");

  pk = vrf_pk_t(json["pk"].asString());
  proof = vrf_proof_t(json["proof"].asString());
  vdf_sol_.first = dev::fromHex(json["sol1"].asString());
  vdf_sol_.second = dev::fromHex(json["sol2"].asString());
  difficulty_ = dev::jsToInt(json["difficulty"].asString());
}

bytes VdfSortition::rlp() const {
  dev::RLPStream s;
  s.appendList(5);
  s << pk;
  s << proof;
  s << vdf_sol_.first;
  s << vdf_sol_.second;
  s << difficulty_;
  return s.out();
}

Json::Value VdfSortition::getJson() const {
  Json::Value res;
  res["pk"] = dev::toJS(pk);
  res["proof"] = dev::toJS(proof);
  res["sol1"] = dev::toJS(dev::toHex(vdf_sol_.first));
  res["sol2"] = dev::toJS(dev::toHex(vdf_sol_.second));
  res["difficulty"] = dev::toJS(difficulty_);
  return res;
}

void VdfSortition::computeVdfSolution(VdfConfig const& config, bytes const& msg) {
  auto t1 = getCurrentTimeMilliSeconds();
  VerifierWesolowski verifier(config.lambda_bound, difficulty_, msg, N);

  ProverWesolowski prover;
  vdf_sol_ = prover(verifier);  // this line takes time ...
  auto t2 = getCurrentTimeMilliSeconds();
  vdf_computation_time_ = t2 - t1;
}

bool VdfSortition::verifyVdf(VdfConfig const& config, bytes const& vrf_input, bytes const& vdf_input) {
  // Verify VRF output
  if (!verifyVrf(vrf_input)) {
    LOG(log_er_) << "VRF verify failed. VDF input " << vdf_input << ", lambda " << config.lambda_bound
                 << ", difficulty " << getDifficulty();
    return false;
  }

  if (difficulty_ != calculateDifficulty(config)) {
    LOG(log_er_) << "VDF solution verification failed. Incorrect difficulty. VDF input " << vdf_input << ", lambda "
                 << config.lambda_bound << ", difficulty " << getDifficulty();
    return false;
  }

  // Verify VDF solution
  VerifierWesolowski verifier(config.lambda_bound, getDifficulty(), vdf_input, N);
  if (!verifier(vdf_sol_)) {
    LOG(log_er_) << "VDF solution verification failed. VDF input " << vdf_input << ", lambda " << config.lambda_bound
                 << ", difficulty " << getDifficulty();
    return false;
  }

  return true;
}

bool VdfSortition::verifyVrf(bytes const& vrf_input) { return VrfSortitionBase::verify(vrf_input); }

uint16_t VdfSortition::getDifficulty() const { return difficulty_; }

}  // namespace taraxa::vdf_sortition