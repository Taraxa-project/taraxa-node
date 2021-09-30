#pragma once

#include <json/json.h>

namespace taraxa {

struct VdfConfig {
  VdfConfig() = default;
  VdfConfig(VdfConfig&&) = default;
  VdfConfig(VdfConfig const& vdf_config)
      : threshold_selection(vdf_config.threshold_selection),
        threshold_vdf_omit(vdf_config.threshold_vdf_omit),
        difficulty_min(vdf_config.difficulty_min),
        difficulty_max(vdf_config.difficulty_max),
        difficulty_stale(vdf_config.difficulty_stale),
        lambda_bound(vdf_config.lambda_bound) {}
  VdfConfig(uint16_t const selection, uint16_t const threshold_vdf_omit, uint16_t const min, uint16_t const max,
            uint16_t const stale, uint16_t const lambda_max_bound)
      : threshold_selection(selection),
        threshold_vdf_omit(threshold_vdf_omit),
        difficulty_min(min),
        difficulty_max(max),
        difficulty_stale(stale),
        lambda_bound(lambda_max_bound) {}

  VdfConfig& operator=(VdfConfig&&) = default;
  VdfConfig& operator=(const VdfConfig&) = default;

  friend std::ostream& operator<<(std::ostream& strm, VdfConfig const& vdf_config) {
    strm << " [VDF config] " << std::endl;
    strm << "    threshold selection: " << vdf_config.threshold_selection << std::endl;
    strm << "    threshold vdf ommit: " << vdf_config.threshold_vdf_omit << std::endl;
    strm << "    difficulty minimum: " << vdf_config.difficulty_min << std::endl;
    strm << "    difficulty maximum: " << vdf_config.difficulty_max << std::endl;
    strm << "    difficulty stale: " << vdf_config.difficulty_stale << std::endl;
    strm << "    lambda bound: " << vdf_config.lambda_bound << std::endl;
    return strm;
  }

  uint16_t threshold_selection = 0;
  uint16_t threshold_vdf_omit = 0;
  uint16_t difficulty_min = 0;
  uint16_t difficulty_max = 1;
  uint16_t difficulty_stale = 0;
  uint16_t lambda_bound = 1500;  // lambda upper bound, should be constant
};
Json::Value enc_json(VdfConfig const& obj);
void dec_json(Json::Value const& json, VdfConfig& obj);

}  // namespace taraxa