#pragma once

#include <json/json.h>

#include <functional>
#include <unordered_map>

#include "common/lazy.hpp"
#include "config/final_chain.hpp"
#include "config/pbft.hpp"
#include "dag/dag_block.hpp"

namespace taraxa::chain_config {
using std::string;
using std::unordered_map;
using ::taraxa::util::lazy::LazyVal;

struct ChainConfig {
  uint64_t chain_id = 0;
  DagBlock dag_genesis_block;
  VdfConfig vdf;
  PbftConfig pbft;
  final_chain::Config final_chain;

 private:
  static LazyVal<std::unordered_map<string, ChainConfig>> const predefined_;

 public:
  ChainConfig() = default;
  ChainConfig(ChainConfig&&) = default;
  ChainConfig(const ChainConfig&) = default;
  ChainConfig& operator=(ChainConfig&&) = default;
  ChainConfig& operator=(const ChainConfig&) = default;

  static auto const& predefined(std::string const& name = "default") {
    if (auto i = predefined_->find(name); i != predefined_->end()) {
      return i->second;
    }
    throw std::runtime_error("unknown chain config: " + name);
  }
};

Json::Value enc_json(ChainConfig const& obj);
void dec_json(Json::Value const& json, ChainConfig& obj);

}  // namespace taraxa::chain_config

namespace taraxa {
using chain_config::ChainConfig;
}
