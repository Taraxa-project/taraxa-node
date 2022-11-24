#pragma once

#include <json/json.h>

#include <functional>
#include <unordered_map>

#include "common/lazy.hpp"
#include "config/dag_config.hpp"
#include "config/pbft_config.hpp"
#include "config/state_config.hpp"
#include "dag/dag_block.hpp"

namespace taraxa {
using std::string;
using std::unordered_map;
using ::taraxa::util::lazy::LazyVal;

struct GasPriceConfig {
  uint64_t percentile = 60;
  uint64_t blocks = 200;
  uint64_t minimum_price = 1;
  void validate() const;
  bytes rlp() const;
};

Json::Value enc_json(GasPriceConfig const& obj);
void dec_json(Json::Value const& json, GasPriceConfig& obj);

struct Genesis {
  uint64_t chain_id = 0;
  DagBlock dag_genesis_block;
  GasPriceConfig gas_price;
  SortitionConfig sortition;
  PbftConfig pbft;
  state_api::Config state;
  DagConfig dag;

 private:
  static LazyVal<std::unordered_map<string, Genesis>> const predefined_;

 public:
  void validate() const;
  bytes rlp() const;
  blk_hash_t genesisHash() const;
  static const Genesis& predefined(std::string const& name = "default");
};

Json::Value enc_json(Genesis const& obj);
void dec_json(Json::Value const& json, Genesis& obj);

}  // namespace taraxa
