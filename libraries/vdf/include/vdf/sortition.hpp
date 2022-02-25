#pragma once

#include <algorithm>

#include "ProverWesolowski.h"
#include "common/types.hpp"
#include "common/vrf_wrapper.hpp"
#include "libdevcore/CommonData.h"
#include "logger/logger.hpp"
#include "openssl/bn.h"
#include "vdf/config.hpp"

namespace taraxa::vdf_sortition {

using namespace vdf;
using namespace vrf_wrapper;

// It includes a vrf for difficulty adjustment
class VdfSortition : public vrf_wrapper::VrfSortitionBase {
 public:
  struct InvalidVdfSortition : std::runtime_error {
    explicit InvalidVdfSortition(std::string const& msg) : runtime_error("Invalid VdfSortition: " + msg) {}
  };

  VdfSortition() = default;
  explicit VdfSortition(SortitionParams const& config, vrf_sk_t const& sk, bytes const& vrf_input);
  explicit VdfSortition(bytes const& b);
  explicit VdfSortition(Json::Value const& json);

  void computeVdfSolution(SortitionParams const& config, dev::bytes const& msg);
  void verifyVdf(SortitionParams const& config, bytes const& vrf_input, bytes const& vdf_input) const;

  bytes rlp() const;
  bool operator==(VdfSortition const& other) const {
    return vrf_wrapper::VrfSortitionBase::operator==(other) && vdf_sol_.first == other.vdf_sol_.first &&
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
  uint16_t calculateDifficulty(SortitionParams const& config) const;
  bool isOmitVdf(SortitionParams const& config) const;
  bool isStale(SortitionParams const& config) const;
  Json::Value getJson() const;

 private:
  inline static dev::bytes N = dev::asBytes(
      "3d1055a514e17cce1290ccb5befb256b00b8aac664e39e754466fcd631004c9e23d16f23"
      "9aee2a207e5173a7ee8f90ee9ab9b6a745d27c6e850e7ca7332388dfef7e5bbe6267d1f7"
      "9f9330e44715b3f2066f903081836c1c83ca29126f8fdc5f5922bf3f9ddb4540171691ac"
      "cc1ef6a34b2a804a18159c89c39b16edee2ede35");
  bool verifyVrf(bytes const& vrf_input) const;

  std::pair<bytes, bytes> vdf_sol_;
  unsigned long vdf_computation_time_ = 0;
  uint16_t difficulty_ = 0;
};

}  // namespace taraxa::vdf_sortition
