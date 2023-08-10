#pragma once

#include <libdevcore/CommonJS.h>

#include "common/encoding_rlp.hpp"
#include "common/types.hpp"

struct Redelegation {
  taraxa::addr_t validator;
  taraxa::addr_t delegator;
  taraxa::uint256_t amount;
  HAS_RLP_FIELDS
};

Json::Value enc_json(const Redelegation& obj);
void dec_json(const Json::Value& json, Redelegation& obj);

struct Hardforks {
  uint64_t fix_redelegate_block_num = -1;
  std::vector<Redelegation> redelegations;
  HAS_RLP_FIELDS
};

Json::Value enc_json(const Hardforks& obj);
void dec_json(const Json::Value& json, Hardforks& obj);