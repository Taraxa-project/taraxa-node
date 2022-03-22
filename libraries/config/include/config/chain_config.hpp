#pragma once

#include <json/json.h>

#include <functional>
#include <unordered_map>

#include "common/lazy.hpp"
#include "config/final_chain_config.hpp"
#include "config/pbft_config.hpp"
#include "dag/dag_block.hpp"

namespace taraxa::chain_config {
using std::string;
using std::unordered_map;
using ::taraxa::util::lazy::LazyVal;

struct GasPriceConfig {
  uint64_t percentile = 60;
  uint64_t blocks = 200;
  void validate() const;
};

Json::Value enc_json(GasPriceConfig const& obj);
void dec_json(Json::Value const& json, GasPriceConfig& obj);

struct ChainConfig {
  uint64_t chain_id = 0;
  DagBlock dag_genesis_block;
  GasPriceConfig gas_price;
  SortitionConfig sortition;
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

  void validate() const;
  static const ChainConfig& predefined(std::string const& name = "default");
};

Json::Value enc_json(ChainConfig const& obj);
void dec_json(Json::Value const& json, ChainConfig& obj);

}  // namespace taraxa::chain_config

namespace taraxa {
using chain_config::ChainConfig;
}
