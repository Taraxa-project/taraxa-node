#include "dag/vdf_sortition.hpp"

#include <libdevcore/CommonData.h>
#include <libdevcore/CommonJS.h>

#include "config/config.hpp"

namespace taraxa::vdf_sortition {

Json::Value enc_json(VdfConfig const& obj) {
  Json::Value ret(Json::objectValue);
  ret["threshold_selection"] = dev::toJS(obj.threshold_selection);
  ret["threshold_vdf_omit"] = dev::toJS(obj.threshold_vdf_omit);
  ret["difficulty_min"] = dev::toJS(obj.difficulty_min);
  ret["difficulty_max"] = dev::toJS(obj.difficulty_max);
  ret["difficulty_stale"] = dev::toJS(obj.difficulty_stale);
  ret["lambda_bound"] = dev::toJS(obj.lambda_bound);
  return ret;
}

void dec_json(Json::Value const& json, VdfConfig& obj) {
  obj.threshold_selection = dev::jsToInt(json["threshold_selection"].asString());
  obj.threshold_vdf_omit = dev::jsToInt(json["threshold_vdf_omit"].asString());
  obj.difficulty_min = dev::jsToInt(json["difficulty_min"].asString());
  obj.difficulty_max = dev::jsToInt(json["difficulty_max"].asString());
  obj.difficulty_stale = dev::jsToInt(json["difficulty_stale"].asString());
  obj.lambda_bound = dev::jsToInt(json["lambda_bound"].asString());
}

VdfSortition::VdfSortition(VdfConfig const& config, vrf_sk_t const& sk, bytes const& msg) : VrfSortitionBase(sk, msg) {
  difficulty_ = calculateDifficulty(config);
}

bool VdfSortition::omitVdf(VdfConfig const& config) const { return threshold <= config.threshold_vdf_omit; }

bool VdfSortition::isStale(VdfConfig const& config) const { return threshold > config.threshold_selection; }

uint16_t VdfSortition::calculateDifficulty(VdfConfig const& config) const {
  uint16_t difficulty = 0;
  if (!omitVdf(config)) {
    if (isStale(config)) {
      difficulty = config.difficulty_stale;
    } else {
      difficulty = config.difficulty_min + threshold % (config.difficulty_max - config.difficulty_min);
    }
  }
  return difficulty;
}

VdfSortition::VdfSortition(bytes const& b) {
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

VdfSortition::VdfSortition(Json::Value const& json) {
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
  if (!omitVdf(config)) {
    auto t1 = getCurrentTimeMilliSeconds();
    VerifierWesolowski verifier(config.lambda_bound, difficulty_, msg, N);

    ProverWesolowski prover;
    vdf_sol_ = prover(verifier);  // this line takes time ...
    auto t2 = getCurrentTimeMilliSeconds();
    vdf_computation_time_ = t2 - t1;
  }
}

void VdfSortition::verifyVdf(VdfConfig const& config, bytes const& vrf_input, bytes const& vdf_input) {
  // Verify VRF output
  if (!verifyVrf(vrf_input)) {
    throw InvalidVdfSortition("VRF verify failed. VDF input " + bytes2str(vdf_input) + ", lambda " +
                              std::to_string(config.lambda_bound) + ", difficulty " + std::to_string(getDifficulty()));
  }

  if (!omitVdf(config)) {
    if (difficulty_ != calculateDifficulty(config)) {
      throw InvalidVdfSortition("VDF solution verification failed. Incorrect difficulty. VDF input " +
                                bytes2str(vdf_input) + ", lambda " + std::to_string(config.lambda_bound) +
                                ", difficulty " + std::to_string(getDifficulty()));
    }

    // Verify VDF solution
    VerifierWesolowski verifier(config.lambda_bound, getDifficulty(), vdf_input, N);
    if (!verifier(vdf_sol_)) {
      throw InvalidVdfSortition("VDF solution verification failed. VDF input " + bytes2str(vdf_input) + ", lambda " +
                                std::to_string(config.lambda_bound) + ", difficulty " +
                                std::to_string(getDifficulty()));
    }
  }
}

bool VdfSortition::verifyVrf(bytes const& vrf_input) { return VrfSortitionBase::verify(vrf_input); }

uint16_t VdfSortition::getDifficulty() const { return difficulty_; }

}  // namespace taraxa::vdf_sortition