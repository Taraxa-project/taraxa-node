#ifndef TARAXA_NODE_CHAIN_CONFIG_HPP_
#define TARAXA_NODE_CHAIN_CONFIG_HPP_

#include <json/json.h>
#include <libethcore/BlockHeader.h>

#include <functional>
#include <unordered_map>

#include "dag_block.hpp"
#include "final_chain.hpp"
#include "util/lazy.hpp"

namespace taraxa::chain_config {
using dev::eth::BlockHeaderFields;
using std::function;
using std::string;
using std::unordered_map;
using ::taraxa::util::lazy::Lazy;
using ::taraxa::util::lazy::LazyVal;

struct ChainConfig {
  int chain_id;
  DagBlock dag_genesis_block;
  FinalChain::Config final_chain;

  static LazyVal<ChainConfig> const Default;
};

}  // namespace taraxa::chain_config

namespace taraxa {
using chain_config::ChainConfig;
}

#endif  // TARAXA_NODE_CHAIN_CONFIG_HPP_
