#pragma once

#include <libdevcore/CommonJS.h>

struct Hardforks {
  uint64_t enable_vrf_adjustion_block = 0;
};

Json::Value enc_json(const Hardforks& obj);
void dec_json(const Json::Value& json, Hardforks& obj);