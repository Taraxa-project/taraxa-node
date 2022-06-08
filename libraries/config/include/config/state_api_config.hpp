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
Json::Value enc_json(const ETHChainConfig& obj);
void dec_json(const Json::Value& json, ETHChainConfig& obj);

using BalanceMap = std::unordered_map<addr_t, u256>;
Json::Value enc_json(const BalanceMap& obj);
void dec_json(const Json::Value& json, BalanceMap& obj);

struct ValidatorInfo {
  addr_t address;
  addr_t owner;
  uint16_t commission = 0;
  std::string endpoint;
  std::string description;
  BalanceMap delegations;

  HAS_RLP_FIELDS
};
Json::Value enc_json(const ValidatorInfo& obj);
void dec_json(const Json::Value& json, ValidatorInfo& obj);

struct DPOSConfig {
  u256 eligibility_balance_threshold;
  u256 vote_eligibility_balance_step;
  u256 validator_maximum_stake;
  u256 minimum_deposit;
  uint16_t commission_change_delta = 0;
  uint32_t commission_change_frequency = 0;  // number of blocks
  uint32_t delegation_delay = 0;             // number of blocks
  uint32_t delegation_locking_period = 0;    // number of blocks
  uint32_t blocks_per_year = 0;              // number of blocks - it is calculated from lambda_ms_min
  uint16_t yield_percentage = 0;             // [%]
  std::vector<ValidatorInfo> initial_validators;

  HAS_RLP_FIELDS
};
Json::Value enc_json(const DPOSConfig& obj);
void dec_json(const Json::Value& json, DPOSConfig& obj);

// This struct has strict ordering, do not change it
struct ExecutionOptions {
  bool disable_nonce_check = false;
  bool enable_nonce_skipping = false;

  HAS_RLP_FIELDS
};
Json::Value enc_json(const ExecutionOptions& obj);
void dec_json(const Json::Value& json, ExecutionOptions& obj);

struct BlockRewardsOptions {
  // Disables new tokens generation as block reward
  bool disable_block_rewards = false;

  // TODO: once we fix tests, this flag can be deleted as rewards should be processed only in dpos contract
  // Disbales rewards distribution through contract - rewards are added directly to the validators accounts
  bool disable_contract_distribution = false;

  HAS_RLP_FIELDS
};
Json::Value enc_json(BlockRewardsOptions const& obj);
void dec_json(Json::Value const& json, BlockRewardsOptions& obj);

struct Config {
  ETHChainConfig eth_chain_config;
  ExecutionOptions execution_options;
  BlockRewardsOptions block_rewards_options;
  BalanceMap genesis_balances;
  std::optional<DPOSConfig> dpos;
  // Hardforks hardforks;

  HAS_RLP_FIELDS
};
Json::Value enc_json(const Config& obj);
void dec_json(const Json::Value& json, Config& obj);

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
