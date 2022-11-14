#pragma once

#include <json/json.h>

#include "common/types.hpp"

namespace taraxa {

struct PbftConfig {
  uint32_t lambda_ms_min = 0;
  uint32_t committee_size = 0;
  uint32_t number_of_proposers = 20;
  uint64_t gas_limit = 0;

  bytes rlp() const;
};
Json::Value enc_json(PbftConfig const& obj);
void dec_json(Json::Value const& json, PbftConfig& obj);

}  // namespace taraxa
