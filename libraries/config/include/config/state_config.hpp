#pragma once

#include <string>
#include <unordered_map>

#include "common/encoding_rlp.hpp"
#include "common/types.hpp"
#include "common/vrf_wrapper.hpp"
#include "config/hardfork.hpp"

namespace taraxa::state_api {

static constexpr auto BlockNumberNIL = std::numeric_limits<EthBlockNumber>::max();

struct EVMChainConfig {
  uint64_t chain_id = 0;
  HAS_RLP_FIELDS
};
Json::Value enc_json(const EVMChainConfig& obj);
void dec_json(const Json::Value& json, EVMChainConfig& obj);

using BalanceMap = std::map<addr_t, u256>;
Json::Value enc_json(const BalanceMap& obj);
void dec_json(const Json::Value& json, BalanceMap& obj);

struct ValidatorInfo {
  addr_t address;
  addr_t owner;
  vrf_wrapper::vrf_pk_t vrf_key;
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
  uint16_t max_block_author_reward = 0;
  uint16_t dag_proposers_reward = 0;
  uint16_t commission_change_delta = 0;
  uint32_t commission_change_frequency = 0;  // number of blocks
  uint32_t delegation_delay = 5;             // number of blocks
  uint32_t delegation_locking_period = 5;    // number of blocks
  uint32_t blocks_per_year = 0;              // number of blocks - it is calculated from lambda_ms
  uint16_t yield_percentage = 0;             // [%]
  std::vector<ValidatorInfo> initial_validators;

  HAS_RLP_FIELDS
};
Json::Value enc_json(const DPOSConfig& obj);
void dec_json(const Json::Value& json, DPOSConfig& obj);

struct Config {
  EVMChainConfig evm_chain_config;
  BalanceMap initial_balances;
  DPOSConfig dpos;
  // Hardforks hardforks;

  HAS_RLP_FIELDS
};
void append_json(Json::Value& json, const Config& obj);
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
