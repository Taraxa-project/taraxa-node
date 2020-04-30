#ifndef TARAXA_NODE_CONF_CHAIN_CONFIG_HPP_
#define TARAXA_NODE_CONF_CHAIN_CONFIG_HPP_

#include <json/json.h>
#include <libethcore/BlockHeader.h>

#include <functional>
#include <unordered_map>

#include "dag_block.hpp"
#include "taraxa-evm/state_api.hpp"
#include "util/lazy.hpp"

namespace taraxa::conf::chain_config {
using dev::eth::BlockHeaderFields;
using std::function;
using std::string;
using std::unordered_map;
using ::taraxa::util::lazy::Lazy;
using ::taraxa::util::lazy::LazyVal;

struct ChainConfig {
  int chain_id;
  DagBlock dag_genesis_block;
  round_t replay_protection_service_range;
  taraxa_evm::state_api::GenesisAccounts genesis_accounts;
  taraxa_evm::state_api::ETHChainConfig eth_chain_config;
  dev::eth::BlockHeaderFields genesis_block;
};

extern LazyVal<ChainConfig> const Default;

}  // namespace taraxa::conf::chain_config

#endif  // TARAXA_NODE_CONF_CHAIN_CONFIG_HPP_
