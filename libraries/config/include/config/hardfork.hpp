#pragma once

#include <libdevcore/CommonJS.h>

#include "common/encoding_rlp.hpp"

using RedelegationMap = std::map<taraxa::addr_t, taraxa::addr_t>;
Json::Value enc_json(const RedelegationMap& obj);
void dec_json(const Json::Value& json, RedelegationMap& obj);

struct Hardforks {
  uint64_t fix_redelegate_block_num = -1;
  RedelegationMap redelegations;
  HAS_RLP_FIELDS
};

Json::Value enc_json(const Hardforks& obj);
void dec_json(const Json::Value& json, Hardforks& obj);