#ifndef TARAXA_NODE_CHAIN_CONFIG_HPP_
#define TARAXA_NODE_CHAIN_CONFIG_HPP_

#include <json/json.h>
#include <libethcore/BlockHeader.h>

#include <functional>
#include <unordered_map>

#include "dag_block.hpp"
#include "final_chain.hpp"
#include "pbft_config.hpp"
#include "util/lazy.hpp"

namespace taraxa::chain_config {
using dev::eth::BlockHeaderFields;
using std::function;
using std::string;
using std::unordered_map;
using ::taraxa::util::lazy::Lazy;
using ::taraxa::util::lazy::LazyVal;

struct ChainConfig {
  uint64_t chain_id = 0;
  DagBlock dag_genesis_block;
  ReplayProtectionService::Config replay_protection_service;
  PbftConfig pbft;
  FinalChain::Config final_chain;

  static LazyVal<addr_t> const default_chain_boot_node_addr;
  inline static u256 const default_chain_boot_node_initial_balance =
      9007199254740991;
  static LazyVal<vector<addr_t>> const default_chain_predefined_nodes;

 private:
  static LazyVal<std::unordered_map<string, ChainConfig>> const predefined_;

 public:
  static auto const& predefined(std::string const& name = "default") {
    if (auto i = predefined_->find(name); i != predefined_->end()) {
      return i->second;
    }
    throw std::runtime_error("unknown chain config: " + name);
  }
};

Json::Value enc_json(ChainConfig const& obj);
void dec_json(Json::Value const& json, ChainConfig& obj);

}  // namespace taraxa::chain_config

namespace taraxa {
using chain_config::ChainConfig;
}

#endif  // TARAXA_NODE_CHAIN_CONFIG_HPP_
