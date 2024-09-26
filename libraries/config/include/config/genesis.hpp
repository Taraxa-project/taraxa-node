#pragma once

#include <json/json.h>

#include "config/dag_config.hpp"
#include "config/pbft_config.hpp"
#include "config/state_config.hpp"
#include "dag/dag_block.hpp"

namespace taraxa {

struct GasPriceConfig {
  uint64_t percentile = 60;
  uint64_t blocks = 200;
  uint64_t minimum_price = 1;
  void validate() const;
  bytes rlp() const;
};

Json::Value enc_json(GasPriceConfig const& obj);
void dec_json(Json::Value const& json, GasPriceConfig& obj);

struct GenesisConfig {
  uint64_t chain_id = 0;
  DagBlock dag_genesis_block;
  GasPriceConfig gas_price;
  SortitionConfig sortition;
  PbftConfig pbft;
  state_api::Config state;
  DagConfig dag;

  GenesisConfig();
  void validate() const;
  bytes rlp() const;
  blk_hash_t genesisHash() const;
  void updateBlocksPerYear();
};

Json::Value enc_json(GenesisConfig const& obj);
void dec_json(Json::Value const& json, GenesisConfig& obj);

}  // namespace taraxa
