#pragma once

#include <string>
#include <unordered_map>

#include "common/encoding_rlp.hpp"
#include "common/types.hpp"
#include "config/hardfork.hpp"

namespace taraxa::state_api {

static constexpr auto BlockNumberNIL = std::numeric_limits<EthBlockNumber>::max();

struct ETHChainConfig {
  EthBlockNumber homestead_block = 0;
  EthBlockNumber dao_fork_block = 0;
  EthBlockNumber eip_150_block = 0;
  EthBlockNumber eip_158_block = 0;
  EthBlockNumber byzantium_block = 0;
  EthBlockNumber constantinople_block = 0;
  EthBlockNumber petersburg_block = 0;

  HAS_RLP_FIELDS
};
Json::Value enc_json(ETHChainConfig const& obj);
void dec_json(Json::Value const& json, ETHChainConfig& obj);

using BalanceMap = std::unordered_map<addr_t, u256>;
Json::Value enc_json(BalanceMap const& obj);
void dec_json(Json::Value const& json, BalanceMap& obj);

struct DPOSConfig {
  u256 eligibility_balance_threshold;
  EthBlockNumber deposit_delay = 0;
  EthBlockNumber withdrawal_delay = 0;
  std::unordered_map<addr_t, BalanceMap> genesis_state;

  HAS_RLP_FIELDS
};
Json::Value enc_json(DPOSConfig const& obj);
void dec_json(Json::Value const& json, DPOSConfig& obj);

struct ExecutionOptions {
  bool disable_nonce_check = 0;
  bool disable_gas_fee = 0;

  HAS_RLP_FIELDS
};
Json::Value enc_json(ExecutionOptions const& obj);
void dec_json(Json::Value const& json, ExecutionOptions& obj);

struct Config {
  ETHChainConfig eth_chain_config;
  bool disable_block_rewards = 0;
  ExecutionOptions execution_options;
  BalanceMap genesis_balances;
  std::optional<DPOSConfig> dpos;
  Hardforks hardforks;

  HAS_RLP_FIELDS

  u256 effective_genesis_balance(addr_t const& addr) const;
};
Json::Value enc_json(Config const& obj);
void dec_json(Json::Value const& json, Config& obj);

struct Opts {
  uint32_t expected_max_trx_per_block = 0;
  uint8_t max_trie_full_node_levels_to_cache = 0;

  HAS_RLP_FIELDS
};

struct OptsDB {
  std::string db_path;
  bool disable_most_recent_trie_value_views = 0;

  HAS_RLP_FIELDS
};

}  // namespace taraxa::state_api
