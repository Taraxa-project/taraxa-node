#pragma once

#include <json/json.h>

#include "common/types.hpp"
#include "config/state_api.hpp"

namespace taraxa::final_chain {

struct Config {
  state_api::Config state;
  struct GenesisBlockFields {
    addr_t author;
    uint64_t timestamp = 0;
  } genesis_block_fields;
};
Json::Value enc_json(Config const& obj);
void dec_json(Json::Value const& json, Config& obj);
Json::Value enc_json(Config::GenesisBlockFields const& obj);
void dec_json(Json::Value const& json, Config::GenesisBlockFields& obj);

}  // namespace taraxa::final_chain