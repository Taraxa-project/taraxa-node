#pragma once
#include "ProverWesolowski.h"
#include "libdevcore/CommonData.h"
#include "openssl/bn.h"
#include "types.hpp"
#include "vrf_wrapper.hpp"

namespace taraxa::vdf_sortition {
using namespace vdf;
using namespace vrf_wrapper;
struct VdfMsg : public vrf_wrapper::VrfMsgFace {
  VdfMsg() = default;
  VdfMsg(blk_hash_t last_pbft_hash, uint64_t level)
      : last_pbft_hash(last_pbft_hash), level(level) {}
  std::string toString() const {
    return last_pbft_hash.toString() + "_" + std::to_string(level);
  }
  bool operator==(VdfMsg const& other) const {
    return last_pbft_hash == other.last_pbft_hash && level == other.level;
  }
  friend std::ostream& operator<<(std::ostream& strm, VdfMsg const& vdf_msg) {
    strm << "  [Vdf Msg] " << std::endl;
    strm << "    last_pbft_hash: " << vdf_msg.last_pbft_hash << std::endl;
    strm << "    level: " << vdf_msg.level << std::endl;
    return strm;
  }
  blk_hash_t last_pbft_hash;
  uint64_t level;
};
// It includes a vrf for difficulty adjustment
struct VdfSortition : public vrf_wrapper::VrfSortitionBase {
  inline static unsigned long const lambda = 2000;
  inline static dev::bytes N = dev::asBytes(
      "3d1055a514e17cce1290ccb5befb256b00b8aac664e39e754466fcd631004c9e23d16f23"
      "9aee2a207e5173a7ee8f90ee9ab9b6a745d27c6e850e7ca7332388dfef7e5bbe6267d1f7"
      "9f9330e44715b3f2066f903081836c1c83ca29126f8fdc5f5922bf3f9ddb4540171691ac"
      "cc1ef6a34b2a804a18159c89c39b16edee2ede35");
  VdfSortition() = default;
  VdfSortition(vrf_sk_t const& sk, VdfMsg const& vdf_msg)
      : vdf_msg(vdf_msg), VrfSortitionBase(sk, vdf_msg) {}
  bool verify() { return VrfSortitionBase::verify(vdf_msg); }
  bool operator==(VdfSortition const& other) const {
    return pk == other.pk && vdf_msg == other.vdf_msg && proof == other.proof &&
           output == other.output;
  }
  VdfMsg vdf_msg;
  void computeVdfSolution();
  bool verifyVdfSolution();
  SolutionWesolowski vdf_sol;
};
}  // namespace taraxa::vdf_sortition