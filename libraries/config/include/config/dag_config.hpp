#pragma once

#include <json/json.h>

#include "common/types.hpp"

namespace taraxa {

struct DagBlockProposerConfig {
  uint16_t shard = 1;

  bytes rlp() const;
};
Json::Value enc_json(const DagBlockProposerConfig& obj);
void dec_json(const Json::Value& json, DagBlockProposerConfig& obj);

struct DagConfig {
  uint64_t gas_limit = 0;
  DagBlockProposerConfig block_proposer;

  bytes rlp() const;
};
Json::Value enc_json(const DagConfig& obj);
void dec_json(const Json::Value& json, DagConfig& obj);

}  // namespace taraxa
