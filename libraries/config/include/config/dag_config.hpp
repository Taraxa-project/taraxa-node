#pragma once

#include <json/json.h>

#include "common/types.hpp"

namespace taraxa {

struct DagConfig {
  u256 gas_limit = 0x989680;
};
Json::Value enc_json(DagConfig const& obj);
void dec_json(Json::Value const& json, DagConfig& obj);

}  // namespace taraxa
