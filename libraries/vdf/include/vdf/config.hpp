#pragma once

#include <json/json.h>

#include "common/constants.hpp"
#include "common/encoding_rlp.hpp"

namespace taraxa {

struct VrfParams {
  uint16_t threshold_upper = 0;  // upper bound of normal selection
  uint16_t threshold_lower = 0;  // lower bound of normal selection
  VrfParams& operator+=(int32_t change);
};

struct VdfParams {
  uint16_t difficulty_min = 0;
  uint16_t difficulty_max = 1;
  uint16_t difficulty_stale = 0;
  uint16_t lambda_bound = 1500;  // lambda upper bound, should be constant
};

struct SortitionParams {
  SortitionParams() = default;
  SortitionParams(SortitionParams&&) = default;
  SortitionParams(const SortitionParams& config) = default;
  SortitionParams(uint16_t threshold_upper, uint16_t threshold_lower, uint16_t min, uint16_t max, uint16_t stale,
                  uint16_t lambda_max_bound)
      : vrf{threshold_upper, threshold_lower}, vdf{min, max, stale, lambda_max_bound} {}
  SortitionParams(const VrfParams& vrf, const VdfParams& vdf) : vrf{vrf}, vdf{vdf} {}

  SortitionParams& operator=(SortitionParams&&) = default;
  SortitionParams& operator=(const SortitionParams&) = default;

  friend std::ostream& operator<<(std::ostream& strm, const SortitionParams& config) {
    strm << " [VDF config] " << std::endl;
    strm << "    vrf upper threshold: " << config.vrf.threshold_upper << std::endl;
    strm << "    vrf lower threshold: " << config.vrf.threshold_lower << std::endl;
    strm << "    difficulty minimum: " << config.vdf.difficulty_min << std::endl;
    strm << "    difficulty maximum: " << config.vdf.difficulty_max << std::endl;
    strm << "    difficulty stale: " << config.vdf.difficulty_stale << std::endl;
    strm << "    lambda bound: " << config.vdf.lambda_bound << std::endl;
    return strm;
  }
  VrfParams vrf;
  VdfParams vdf;
};

struct SortitionConfig : SortitionParams {
  uint16_t changes_count_for_average = 5;  // intervals
  uint16_t max_interval_correction = 10 * kOnePercent;
  std::pair<uint16_t, uint16_t> dag_efficiency_targets = {48 * kOnePercent, 52 * kOnePercent};
  uint16_t computation_interval = 50;  // pbft blocks

  uint16_t targetEfficiency() const { return (dag_efficiency_targets.first + dag_efficiency_targets.second) / 2; }
};

Json::Value enc_json(const VrfParams& obj);
void dec_json(const Json::Value& json, VrfParams& obj);
Json::Value enc_json(const VdfParams& obj);
void dec_json(const Json::Value& json, VdfParams& obj);

Json::Value enc_json(const SortitionParams& obj);
void dec_json(const Json::Value& json, SortitionParams& obj);

Json::Value enc_json(const SortitionConfig& obj);
void dec_json(const Json::Value& json, SortitionConfig& obj);

}  // namespace taraxa