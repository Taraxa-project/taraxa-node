#pragma once

#include <json/json.h>

#include "common/constants.hpp"
#include "common/encoding_rlp.hpp"

namespace taraxa {

struct VrfParams {
  uint16_t threshold_upper = 0;  // upper bound of selection
  VrfParams& operator+=(int32_t change);

  static constexpr uint16_t kThresholdUpperMinValue = 0x50;
};

struct VdfParams {
  uint16_t difficulty_min = 0;
  uint16_t difficulty_max = 1;
  uint16_t difficulty_stale = 0;
  uint16_t lambda_bound = 1500;  // lambda upper bound, should be constant
  uint16_t stake_threshold =
      20000;  // Range 0 - UINT16_MAX. Decreasing this value, increases dag block production for higher stake nodes
};

struct SortitionParams {
  SortitionParams() = default;
  SortitionParams(uint16_t threshold_upper, uint16_t min, uint16_t max, uint16_t stale, uint16_t lambda_max_bound)
      : vrf{threshold_upper}, vdf{min, max, stale, lambda_max_bound} {}
  SortitionParams(const VrfParams& vrf, const VdfParams& vdf) : vrf{vrf}, vdf{vdf} {}

  friend std::ostream& operator<<(std::ostream& strm, const SortitionParams& config) {
    strm << " [VDF config] " << std::endl;
    strm << "    vrf upper threshold: " << config.vrf.threshold_upper << std::endl;
    strm << "    difficulty minimum: " << config.vdf.difficulty_min << std::endl;
    strm << "    difficulty maximum: " << config.vdf.difficulty_max << std::endl;
    strm << "    difficulty stale: " << config.vdf.difficulty_stale << std::endl;
    strm << "    lambda bound: " << config.vdf.lambda_bound << std::endl;
    strm << "    stake threshold: " << config.vdf.stake_threshold << std::endl;
    return strm;
  }
  VrfParams vrf;
  VdfParams vdf;
};

struct SortitionConfig : SortitionParams {
  uint16_t changes_count_for_average = 10;  // intervals
  std::pair<uint16_t, uint16_t> dag_efficiency_targets = {69 * kOnePercent, 71 * kOnePercent};
  uint16_t changing_interval = 200;    // non empty pbft blocks
  uint16_t computation_interval = 50;  // non empty pbft blocks

  uint16_t targetEfficiency() const { return (dag_efficiency_targets.first + dag_efficiency_targets.second) / 2; }
  bytes rlp() const;
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