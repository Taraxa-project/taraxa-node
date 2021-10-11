#pragma once

#include <json/json.h>

namespace taraxa {

struct VrfParams {
  uint16_t threshold_upper = 0;  // upper bound of normal selection
  uint16_t threshold_lower = 0;  // lower bound of normal selection
};

struct VdfParams {
  uint16_t difficulty_min = 0;
  uint16_t difficulty_max = 1;
  uint16_t difficulty_stale = 0;
  uint16_t lambda_bound = 1500;  // lambda upper bound, should be constant
};

struct SortitionConfig {
  SortitionConfig() = default;
  SortitionConfig(SortitionConfig&&) = default;
  SortitionConfig(SortitionConfig const& config) = default;
  SortitionConfig(uint16_t const selection, uint16_t const threshold_lower, uint16_t const min, uint16_t const max,
                  uint16_t const stale, uint16_t const lambda_max_bound)
      : vrf{selection, threshold_lower}, vdf{min, max, stale, lambda_max_bound} {}

  SortitionConfig& operator=(SortitionConfig&&) = default;
  SortitionConfig& operator=(const SortitionConfig&) = default;

  friend std::ostream& operator<<(std::ostream& strm, SortitionConfig const& config) {
    strm << " [VDF config] " << std::endl;
    strm << "    threshold selection: " << config.vrf.threshold_upper << std::endl;
    strm << "    threshold vdf ommit: " << config.vrf.threshold_lower << std::endl;
    strm << "    difficulty minimum: " << config.vdf.difficulty_min << std::endl;
    strm << "    difficulty maximum: " << config.vdf.difficulty_max << std::endl;
    strm << "    difficulty stale: " << config.vdf.difficulty_stale << std::endl;
    strm << "    lambda bound: " << config.vdf.lambda_bound << std::endl;
    return strm;
  }
  VrfParams vrf;
  VdfParams vdf;
};

Json::Value enc_json(VrfParams const& obj);
void dec_json(Json::Value const& json, VrfParams& obj);
Json::Value enc_json(VdfParams const& obj);
void dec_json(Json::Value const& json, VdfParams& obj);
Json::Value enc_json(SortitionConfig const& obj);
void dec_json(Json::Value const& json, SortitionConfig& obj);

}  // namespace taraxa