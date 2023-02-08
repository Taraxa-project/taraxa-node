#include "vdf/sortition.hpp"

#include <libdevcore/CommonData.h>
#include <libdevcore/CommonJS.h>

#include <memory>

namespace taraxa::vdf_sortition {

VdfSortition::VdfSortition(const SortitionParams& config, const vrf_sk_t& sk, const bytes& vrf_input,
                           uint64_t vote_count, uint64_t total_vote_count)
    : VrfSortitionBase(sk, vrf_input) {
  difficulty_ = calculateDifficulty(config, vote_count, total_vote_count);
}

bool VdfSortition::isStale(SortitionParams const& config) const { return difficulty_ == config.vdf.difficulty_stale; }

uint16_t VdfSortition::calculateDifficulty(SortitionParams const& config, uint64_t vote_count,
                                           uint64_t total_vote_count) const {
  uint16_t difficulty = 0;
  if (threshold_ >= config.vrf.threshold_upper) {
    difficulty = config.vdf.difficulty_stale;
  } else {
    const auto number_of_difficulties = config.vdf.difficulty_max - config.vdf.difficulty_min + 1;
    difficulty = config.vdf.difficulty_min + threshold_ / (config.vrf.threshold_upper / number_of_difficulties);
  }

  // Nodes with higher stake have greater chance to decrease difficulty to minimum
  // This is a method to allow nodes with higher stake to produce more DAG blocks
  // The values for this selection were chosen to keep the dag production fluid and avoid stale blocks
  // but at the same time reward higher stake
  const auto decrease_probability = vote_count * UINT16_MAX / total_vote_count;
  if ((threshold_ % config.vdf.stake_threshold) < decrease_probability) {
    difficulty = config.vdf.difficulty_min;
  }

  return difficulty;
}

VdfSortition::VdfSortition(bytes const& b) {
  if (b.empty()) {
    return;
  }

  dev::RLP rlp(b);
  util::rlp_tuple(util::RLPDecoderRef(rlp, true), proof_, vdf_sol_.first, vdf_sol_.second, difficulty_);
}

VdfSortition::VdfSortition(Json::Value const& json) {
  proof_ = vrf_proof_t(json["proof"].asString());
  vdf_sol_.first = dev::fromHex(json["sol1"].asString());
  vdf_sol_.second = dev::fromHex(json["sol2"].asString());
  difficulty_ = dev::jsToInt(json["difficulty"].asString());
}

bytes VdfSortition::rlp() const {
  dev::RLPStream s;
  s.appendList(4);
  s << proof_;
  s << vdf_sol_.first;
  s << vdf_sol_.second;
  s << difficulty_;
  return s.invalidate();
}

Json::Value VdfSortition::getJson() const {
  Json::Value res;
  res["proof"] = dev::toJS(proof_);
  res["sol1"] = dev::toJS(dev::toHex(vdf_sol_.first));
  res["sol2"] = dev::toJS(dev::toHex(vdf_sol_.second));
  res["difficulty"] = dev::toJS(difficulty_);
  return res;
}

void VdfSortition::computeVdfSolution(const SortitionParams& config, const bytes& msg,
                                      const std::atomic_bool& cancelled) {
  auto t1 = getCurrentTimeMilliSeconds();
  VerifierWesolowski verifier(config.vdf.lambda_bound, difficulty_, msg, N);
  ProverWesolowski prover;
  vdf_sol_ = prover(verifier, cancelled);  // this line takes time ...
  auto t2 = getCurrentTimeMilliSeconds();
  vdf_computation_time_ = t2 - t1;
}

void VdfSortition::verifyVdf(SortitionParams const& config, bytes const& vrf_input, const vrf_pk_t& pk,
                             bytes const& vdf_input, uint64_t vote_count, uint64_t total_vote_count) const {
  // Verify VRF output
  if (!verifyVrf(pk, vrf_input)) {
    throw InvalidVdfSortition("VRF verify failed. VRF input " + dev::toHex(vrf_input));
  }

  const auto expected = calculateDifficulty(config, vote_count, total_vote_count);
  if (difficulty_ != expected) {
    throw InvalidVdfSortition("VDF solution verification failed. Incorrect difficulty. VDF input " +
                              dev::toHex(vdf_input) + ", lambda " + std::to_string(config.vdf.lambda_bound) +
                              ", difficulty " + std::to_string(getDifficulty()) +
                              ", expected: " + std::to_string(expected) +
                              ", vrf_params: ( threshold_upper: " + std::to_string(config.vrf.threshold_upper) +
                              ") THRESHOLD: " + std::to_string(threshold_));
  }

  // Verify VDF solution
  VerifierWesolowski verifier(config.vdf.lambda_bound, getDifficulty(), vdf_input, N);
  if (!verifier(vdf_sol_)) {
    throw InvalidVdfSortition("VDF solution verification failed. VDF input " + dev::toHex(vdf_input) + ", lambda " +
                              std::to_string(config.vdf.lambda_bound) + ", difficulty " +
                              std::to_string(getDifficulty()));
  }
}

bool VdfSortition::verifyVrf(const vrf_pk_t& pk, const bytes& vrf_input) const {
  return VrfSortitionBase::verify(pk, vrf_input);
}

uint16_t VdfSortition::getDifficulty() const { return difficulty_; }

}  // namespace taraxa::vdf_sortition
