#pragma once

#include <json/json.h>

#include "common/constants.hpp"
#include "common/encoding_rlp.hpp"

namespace taraxa {

struct VrfParams {
  uint16_t threshold_upper = 0;  // upper bound of normal selection
  uint16_t threshold_lower = 0;  // lower bound of normal selection
  VrfParams& operator+=(int change) {
    threshold_upper += change;
    threshold_lower += change;
    return *this;
  }
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
  SortitionParams(SortitionParams const& config) = default;
  SortitionParams(uint16_t const selection, uint16_t const threshold_lower, uint16_t const min, uint16_t const max,
                  uint16_t const stale, uint16_t const lambda_max_bound)
      : vrf{selection, threshold_lower}, vdf{min, max, stale, lambda_max_bound} {}
  SortitionParams(VrfParams vrf, VdfParams vdf) : vrf{vrf}, vdf{vdf} {}

  SortitionParams& operator=(SortitionParams&&) = default;
  SortitionParams& operator=(const SortitionParams&) = default;

  friend std::ostream& operator<<(std::ostream& strm, SortitionParams const& config) {
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
  uint16_t max_interval_correction = 10 * ONE_PERCENT;
  uint16_t target_dag_efficiency = 50 * ONE_PERCENT;
  uint16_t computation_interval = 50;  // pbft blocks
};

Json::Value enc_json(VrfParams const& obj);
void dec_json(Json::Value const& json, VrfParams& obj);
Json::Value enc_json(VdfParams const& obj);
void dec_json(Json::Value const& json, VdfParams& obj);

Json::Value enc_json(SortitionParams const& obj);
void dec_json(Json::Value const& json, SortitionParams& obj);

Json::Value enc_json(SortitionConfig const& obj);
void dec_json(Json::Value const& json, SortitionConfig& obj);

}  // namespace taraxa