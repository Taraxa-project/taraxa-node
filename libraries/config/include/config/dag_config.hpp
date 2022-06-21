#pragma once

#include <json/json.h>

#include "common/types.hpp"

namespace taraxa {

struct BlockProposerConfig {
  uint16_t shard = 1;

  bytes rlp() const;
};
Json::Value enc_json(const BlockProposerConfig& obj);
void dec_json(const Json::Value& json, BlockProposerConfig& obj);

struct DagConfig {
  uint64_t gas_limit = 0;
  BlockProposerConfig block_proposer;

  bytes rlp() const;
};
Json::Value enc_json(const DagConfig& obj);
void dec_json(const Json::Value& json, DagConfig& obj);

}  // namespace taraxa
