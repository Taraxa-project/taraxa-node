#pragma once

#include <libdevcore/CommonJS.h>

#include "common/encoding_rlp.hpp"

struct Hardforks {
  uint64_t fix_state_after_redelegate = 0;
  HAS_RLP_FIELDS
};

Json::Value enc_json(const Hardforks& obj);
void dec_json(const Json::Value& json, Hardforks& obj);