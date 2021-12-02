#pragma once

#include <json/json.h>

#include "common/types.hpp"

namespace taraxa {

struct PbftConfig {
  uint32_t lambda_uint = 0;
  uint32_t committee_size = 0;
  uint32_t max_ghost_size = 0;
  uint32_t ghost_path_move_back = 0;
  bool debug_count_votes = false;
};
Json::Value enc_json(PbftConfig const& obj);
void dec_json(Json::Value const& json, PbftConfig& obj);

}  // namespace taraxa
