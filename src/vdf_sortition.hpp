#ifndef TARAXA_NODE_VDF_SORTITION_H
#define TARAXA_NODE_VDF_SORTITION_H

#include <algorithm>

#include "ProverWesolowski.h"
#include "libdevcore/CommonData.h"
#include "openssl/bn.h"
#include "types.hpp"
#include "util.hpp"
#include "vrf_wrapper.hpp"

namespace taraxa::vdf_sortition {

using namespace vdf;
using namespace vrf_wrapper;

struct Message : public vrf_wrapper::VrfMsgFace {
  Message() = default;
  // Remove proposal anchor hash for VRF message, in order to decouple DAG with
  // PBFT.
  Message(uint64_t level) : level(level) {}

  std::string toString() const override { return std::to_string(level); }
  bool operator==(Message const& other) const { return level == other.level; }
  friend std::ostream& operator<<(std::ostream& strm, Message const& msg) {
    strm << "  [Vdf Msg] " << std::endl;
    strm << "    level: " << msg.level << std::endl;
    return strm;
  }

  uint64_t level = 0;
};

struct VdfConfig {
  VdfConfig() = default;
  VdfConfig(vdf_sortition::VdfConfig const& vdf_config)
      : difficulty_selection(vdf_config.difficulty_selection),
        difficulty_min(vdf_config.difficulty_min),
        difficulty_max(vdf_config.difficulty_max),
        difficulty_stale(vdf_config.difficulty_stale),
        lambda_bound(vdf_config.lambda_bound) {}
  VdfConfig(uint16_t const selection, uint16_t const min, uint16_t const max, uint16_t const stale, uint16_t const lambda_max_bound)
      : difficulty_selection(selection), difficulty_min(min), difficulty_max(max), difficulty_stale(stale), lambda_bound(lambda_max_bound) {}

  friend std::ostream& operator<<(std::ostream& strm, VdfConfig const& vdf_config) {
    strm << " [VDF config] " << std::endl;
    strm << "    difficulty selection: " << vdf_config.difficulty_selection << std::endl;
    strm << "    difficulty minimum: " << vdf_config.difficulty_min << std::endl;
    strm << "    difficulty maximum: " << vdf_config.difficulty_max << std::endl;
    strm << "    difficulty stale: " << vdf_config.difficulty_stale << std::endl;
    strm << "    lambda bound: " << vdf_config.lambda_bound << std::endl;
    return strm;
  }

  uint16_t difficulty_selection = 0;
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
  explicit VdfSortition(VdfConfig const& config, addr_t node_addr, vrf_sk_t const& sk, Message const& msg);
  explicit VdfSortition(addr_t node_addr, bytes const& b);

  bool verify(std::string const& msg) { return verifyVdfSolution(msg); }
  void computeVdfSolution(std::string const& msg);
  bool verifyVdf(level_t propose_block_level, std::string const& vdf_input);

  bytes rlp() const;
  bool operator==(VdfSortition const& other) const {
    return pk == other.pk && msg_ == other.msg_ && proof == other.proof && output == other.output && vdf_sol_.first == other.vdf_sol_.first &&
           vdf_sol_.second == other.vdf_sol_.second;
  }
  bool operator!=(VdfSortition const& other) const { return !operator==(other); }

  virtual std::ostream& print(std::ostream& strm) const override {
    VrfSortitionBase::print(strm);
    strm << msg_ << std::endl;
    strm << " Lambda: " << getLambda() << std::endl;
    strm << " Difficulty: " << getDifficulty() << std::endl;
    strm << " Computation Time: " << vdf_computation_time_ << std::endl;
    strm << " Sol1: " << dev::toHex(vdf_sol_.first) << std::endl;
    strm << " Sol2: " << dev::toHex(vdf_sol_.second) << std::endl;
    return strm;
  }
  friend std::ostream& operator<<(std::ostream& strm, VdfSortition const& vdf) { return vdf.print(strm); }

  Message getVrfMessage() const { return msg_; }
  auto getComputationTime() const { return vdf_computation_time_; }
  uint16_t getDifficulty() const;
  uint16_t getLambda() const;

 private:
  inline static dev::bytes N = dev::asBytes(
      "3d1055a514e17cce1290ccb5befb256b00b8aac664e39e754466fcd631004c9e23d16f23"
      "9aee2a207e5173a7ee8f90ee9ab9b6a745d27c6e850e7ca7332388dfef7e5bbe6267d1f7"
      "9f9330e44715b3f2066f903081836c1c83ca29126f8fdc5f5922bf3f9ddb4540171691ac"
      "cc1ef6a34b2a804a18159c89c39b16edee2ede35");
  bool verifyVrf();
  bool verifyVdfSolution(std::string const& vdf_msg);

  Message msg_;
  std::pair<bytes, bytes> vdf_sol_;
  unsigned long vdf_computation_time_ = 0;
  uint16_t difficulty_selection_ = 0;
  uint16_t difficulty_min_ = 0;
  uint16_t difficulty_max_ = 1;
  uint16_t difficulty_stale_ = 0;
  uint16_t lambda_bound_ = 1500;  // lambda upper bound, should be constant

  LOG_OBJECTS_DEFINE;
};

}  // namespace taraxa::vdf_sortition

#endif  // TARAXA_NODE_VDF_SORTITION_H