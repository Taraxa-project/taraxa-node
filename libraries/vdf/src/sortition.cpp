#include "vdf/sortition.hpp"

#include <libdevcore/CommonData.h>
#include <libdevcore/CommonJS.h>

namespace taraxa::vdf_sortition {

VdfSortition::VdfSortition(const SortitionParams& config, const vrf_sk_t& sk, const bytes& vrf_input)
    : VrfSortitionBase(sk, vrf_input) {
  difficulty_ = calculateDifficulty(config);
}

bool VdfSortition::isOmitVdf(SortitionParams const& config) const {
  return config.vrf.threshold_upper >= config.vrf.threshold_range &&
         threshold_ <= config.vrf.threshold_upper - config.vrf.threshold_range;
}

bool VdfSortition::isStale(SortitionParams const& config) const { return threshold_ > config.vrf.threshold_upper; }

uint16_t VdfSortition::calculateDifficulty(SortitionParams const& config) const {
  uint16_t difficulty = 0;
  if (!isOmitVdf(config)) {
    if (isStale(config)) {
      difficulty = config.vdf.difficulty_stale;
    } else {
      difficulty = config.vdf.difficulty_min + threshold_ % (config.vdf.difficulty_max - config.vdf.difficulty_min);
    }
  }
  return difficulty;
}

VdfSortition::VdfSortition(bytes const& b) {
  if (b.empty()) {
    return;
  }

  dev::RLP rlp(b);
  util::rlp_tuple(util::RLPDecoderRef(rlp, true), pk_, proof_, vdf_sol_.first, vdf_sol_.second, difficulty_);
}

VdfSortition::VdfSortition(Json::Value const& json) {
  pk_ = vrf_pk_t(json["pk"].asString());
  proof_ = vrf_proof_t(json["proof"].asString());
  vdf_sol_.first = dev::fromHex(json["sol1"].asString());
  vdf_sol_.second = dev::fromHex(json["sol2"].asString());
  difficulty_ = dev::jsToInt(json["difficulty"].asString());
}

bytes VdfSortition::rlp() const {
  dev::RLPStream s;
  s.appendList(5);
  s << pk_;
  s << proof_;
  s << vdf_sol_.first;
  s << vdf_sol_.second;
  s << difficulty_;
  return s.invalidate();
}

Json::Value VdfSortition::getJson() const {
  Json::Value res;
  res["pk"] = dev::toJS(pk_);
  res["proof"] = dev::toJS(proof_);
  res["sol1"] = dev::toJS(dev::toHex(vdf_sol_.first));
  res["sol2"] = dev::toJS(dev::toHex(vdf_sol_.second));
  res["difficulty"] = dev::toJS(difficulty_);
  return res;
}

void VdfSortition::computeVdfSolution(SortitionParams const& config, bytes const& msg) {
  if (!isOmitVdf(config)) {
    auto t1 = getCurrentTimeMilliSeconds();
    VerifierWesolowski verifier(config.vdf.lambda_bound, difficulty_, msg, N);

    ProverWesolowski prover;
    vdf_sol_ = prover(verifier);  // this line takes time ...
    auto t2 = getCurrentTimeMilliSeconds();
    vdf_computation_time_ = t2 - t1;
  }
}

void VdfSortition::verifyVdf(SortitionParams const& config, bytes const& vrf_input, bytes const& vdf_input) const {
  // Verify VRF output
  if (!verifyVrf(vrf_input)) {
    throw InvalidVdfSortition("VRF verify failed. VRF input " + bytes2str(vrf_input));
  }

  if (!isOmitVdf(config)) {
    const auto expected = calculateDifficulty(config);
    if (difficulty_ != expected) {
      throw InvalidVdfSortition("VDF solution verification failed. Incorrect difficulty. VDF input " +
                                bytes2str(vdf_input) + ", lambda " + std::to_string(config.vdf.lambda_bound) +
                                ", difficulty " + std::to_string(getDifficulty()) +
                                ", expected: " + std::to_string(expected) +
                                ", vrf_params: ( range: " + std::to_string(config.vrf.threshold_range) +
                                ", threshold_upper: " + std::to_string(config.vrf.threshold_upper) +
                                ") THRESHOLD: " + std::to_string(threshold_));
    }

    // Verify VDF solution
    VerifierWesolowski verifier(config.vdf.lambda_bound, getDifficulty(), vdf_input, N);
    if (!verifier(vdf_sol_)) {
      throw InvalidVdfSortition("VDF solution verification failed. VDF input " + bytes2str(vdf_input) + ", lambda " +
                                std::to_string(config.vdf.lambda_bound) + ", difficulty " +
                                std::to_string(getDifficulty()));
    }
  }
}

bool VdfSortition::verifyVrf(bytes const& vrf_input) const { return VrfSortitionBase::verify(vrf_input); }

uint16_t VdfSortition::getDifficulty() const { return difficulty_; }

}  // namespace taraxa::vdf_sortition