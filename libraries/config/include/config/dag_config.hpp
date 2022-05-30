#pragma once

#include <json/json.h>

#include "common/types.hpp"

namespace taraxa {

struct DagConfig {
  uint64_t gas_limit = 0;
};
Json::Value enc_json(const DagConfig& obj);
void dec_json(const Json::Value& json, DagConfig& obj);

}  // namespace taraxa
