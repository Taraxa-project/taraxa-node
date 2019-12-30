#ifndef TARAXA_NODE_CONF_CHAIN_CONFIG_HPP_
#define TARAXA_NODE_CONF_CHAIN_CONFIG_HPP_

#include <json/json.h>
#include <libethereum/ChainParams.h>

#include <functional>
#include <unordered_map>

#include "dag_block.hpp"
#include "util/lazy.hpp"

namespace taraxa::conf::chain_config {
using dev::eth::ChainParams;
using std::function;
using std::string;
using std::unordered_map;
using ::taraxa::util::lazy::Lazy;
using ::taraxa::util::lazy::LazyVal;

struct ChainConfig {
  round_t replay_protection_service_range;
  DagBlock dag_genesis_block;
  ChainParams eth;

  static ChainConfig from_json(string const& json);
  static ChainConfig from_json(Json::Value const& json);

  static LazyVal<unordered_map<string, ChainConfig>> const PREDEF;
  static auto const& DEFAULT() { return PREDEF->at("default"); }
};

}  // namespace taraxa::conf::chain_config

#endif  // TARAXA_NODE_CONF_CHAIN_CONFIG_HPP_
