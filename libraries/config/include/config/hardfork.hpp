#pragma once

#include <libdevcore/CommonJS.h>

#include "common/encoding_rlp.hpp"

struct Hardforks {
  uint64_t fix_genesis_hardfork_block_num = 0;
  void processFixGenesisHardfork() const;
  HAS_RLP_FIELDS
};

Json::Value enc_json(const Hardforks& obj);
void dec_json(const Json::Value& json, Hardforks& obj);