#pragma once

#include <algorithm>

#include "ProverWesolowski.h"
#include "common/types.hpp"
#include "consensus/vrf_wrapper.hpp"
#include "libdevcore/CommonData.h"
#include "logger/log.hpp"
#include "openssl/bn.h"

namespace taraxa::vdf_sortition {

using namespace vdf;
using namespace vrf_wrapper;

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

// It includes a vrf for difficulty adjustment
class VdfSortition : public vrf_wrapper::VrfSortitionBase {
 public:
  VdfSortition() = default;
  explicit VdfSortition(VdfConfig const& config, addr_t node_addr, vrf_sk_t const& sk, bytes const& msg);
  explicit VdfSortition(addr_t node_addr, bytes const& b);
  explicit VdfSortition(addr_t node_addr, Json::Value const& json);

  void computeVdfSolution(VdfConfig const& config, dev::bytes const& msg);
  bool verifyVdf(VdfConfig const& config, bytes const& vrf_input, bytes const& vdf_input);

  bytes rlp() const;
  bool operator==(VdfSortition const& other) const {
    return pk == other.pk && proof == other.proof && output == other.output && vdf_sol_.first == other.vdf_sol_.first &&
           vdf_sol_.second == other.vdf_sol_.second;
  }
  bool operator!=(VdfSortition const& other) const { return !operator==(other); }

  virtual std::ostream& print(std::ostream& strm) const override {
    VrfSortitionBase::print(strm);
    strm << " Difficulty: " << getDifficulty() << std::endl;
    strm << " Computation Time: " << vdf_computation_time_ << std::endl;
    strm << " Sol1: " << dev::toHex(vdf_sol_.first) << std::endl;
    strm << " Sol2: " << dev::toHex(vdf_sol_.second) << std::endl;
    return strm;
  }
  friend std::ostream& operator<<(std::ostream& strm, VdfSortition const& vdf) { return vdf.print(strm); }

  auto getComputationTime() const { return vdf_computation_time_; }
  uint16_t getDifficulty() const;
  uint16_t calculateDifficulty(VdfConfig const& config) const;
  bool omitVdf(VdfConfig const& config) const;
  bool isStale(VdfConfig const& config) const;
  Json::Value getJson() const;

 private:
  inline static dev::bytes N = dev::asBytes(
      "3d1055a514e17cce1290ccb5befb256b00b8aac664e39e754466fcd631004c9e23d16f23"
      "9aee2a207e5173a7ee8f90ee9ab9b6a745d27c6e850e7ca7332388dfef7e5bbe6267d1f7"
      "9f9330e44715b3f2066f903081836c1c83ca29126f8fdc5f5922bf3f9ddb4540171691ac"
      "cc1ef6a34b2a804a18159c89c39b16edee2ede35");
  bool verifyVrf(bytes const& vrf_input);

  std::pair<bytes, bytes> vdf_sol_;
  unsigned long vdf_computation_time_ = 0;
  uint16_t difficulty_ = 0;

  LOG_OBJECTS_DEFINE
};

}  // namespace taraxa::vdf_sortition
