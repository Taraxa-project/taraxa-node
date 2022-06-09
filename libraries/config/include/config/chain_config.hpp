#pragma once

#include <json/json.h>

#include <functional>
#include <unordered_map>

#include "common/lazy.hpp"
#include "config/dag_config.hpp"
#include "config/final_chain_config.hpp"
#include "config/pbft_config.hpp"
#include "dag/dag_block.hpp"

namespace taraxa {
using std::string;
using std::unordered_map;
using ::taraxa::util::lazy::LazyVal;

struct GasPriceConfig {
  uint64_t percentile = 60;
  uint64_t blocks = 200;
  void validate() const;
  bytes rlp() const;
};

Json::Value enc_json(GasPriceConfig const& obj);
void dec_json(Json::Value const& json, GasPriceConfig& obj);

struct ChainConfig {
  ChainConfig() = default;
  ChainConfig(const std::string& name);

  uint64_t chain_id = 0;
  DagBlock dag_genesis_block;
  GasPriceConfig gas_price;
  SortitionConfig sortition;
  PbftConfig pbft;
  final_chain::Config final_chain;
  DagConfig dag;

  void validate() const;
  bytes rlp() const;
  blk_hash_t genesisHash() const;

 private:
  void setDefaultValue();
  void setTestValue();
};

Json::Value enc_json(ChainConfig const& obj);
void dec_json(Json::Value const& json, ChainConfig& obj);

}  // namespace taraxa
