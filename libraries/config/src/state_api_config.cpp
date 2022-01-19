#include "config/state_api_config.hpp"

#include <libdevcore/CommonJS.h>

#include <sstream>

namespace taraxa::state_api {

Json::Value enc_json(ETHChainConfig const& obj) {
  Json::Value json(Json::objectValue);
  json["homestead_block"] = dev::toJS(obj.homestead_block);
  json["dao_fork_block"] = dev::toJS(obj.dao_fork_block);
  json["eip_150_block"] = dev::toJS(obj.eip_150_block);
  json["eip_158_block"] = dev::toJS(obj.eip_158_block);
  json["byzantium_block"] = dev::toJS(obj.byzantium_block);
  json["constantinople_block"] = dev::toJS(obj.constantinople_block);
  json["petersburg_block"] = dev::toJS(obj.petersburg_block);
  return json;
}

void dec_json(Json::Value const& json, ETHChainConfig& obj) {
  obj.homestead_block = dev::jsToInt(json["homestead_block"].asString());
  obj.dao_fork_block = dev::jsToInt(json["dao_fork_block"].asString());
  obj.eip_150_block = dev::jsToInt(json["eip_150_block"].asString());
  obj.eip_158_block = dev::jsToInt(json["eip_158_block"].asString());
  obj.byzantium_block = dev::jsToInt(json["byzantium_block"].asString());
  obj.constantinople_block = dev::jsToInt(json["constantinople_block"].asString());
  obj.petersburg_block = dev::jsToInt(json["petersburg_block"].asString());
}

Json::Value enc_json(Config const& obj) {
  Json::Value json(Json::objectValue);
  json["eth_chain_config"] = enc_json(obj.eth_chain_config);
  json["execution_options"] = enc_json(obj.execution_options);
  json["disable_block_rewards"] = obj.disable_block_rewards;
  json["genesis_balances"] = enc_json(obj.genesis_balances);
  json["hardforks"] = enc_json(obj.hardforks);
  if (obj.dpos) {
    json["dpos"] = enc_json(*obj.dpos);
  }
  return json;
}

void dec_json(Json::Value const& json, Config& obj) {
  dec_json(json["eth_chain_config"], obj.eth_chain_config);
  dec_json(json["execution_options"], obj.execution_options);
  obj.disable_block_rewards = json["disable_block_rewards"].asBool();
  dec_json(json["genesis_balances"], obj.genesis_balances);
  dec_json(json["hardforks"], obj.hardforks);
  if (auto const& dpos = json["dpos"]; !dpos.isNull()) {
    dec_json(dpos, obj.dpos.emplace());
  }
}

Json::Value enc_json(BalanceMap const& obj) {
  Json::Value json(Json::objectValue);
  for (auto const& [k, v] : obj) {
    json[dev::toJS(k)] = dev::toJS(v);
  }
  return json;
}

void dec_json(Json::Value const& json, BalanceMap& obj) {
  for (auto const& k : json.getMemberNames()) {
    obj[addr_t(k)] = dev::jsToU256(json[k].asString());
  }
}

Json::Value enc_json(DPOSConfig const& obj) {
  Json::Value json(Json::objectValue);
  json["eligibility_balance_threshold"] = dev::toJS(obj.eligibility_balance_threshold);
  json["deposit_delay"] = dev::toJS(obj.deposit_delay);
  json["withdrawal_delay"] = dev::toJS(obj.withdrawal_delay);
  json["vote_eligibility_balance_step"] = dev::toJS(obj.vote_eligibility_balance_step);
  auto& genesis_state = json["genesis_state"] = Json::Value(Json::objectValue);
  for (auto const& [k, v] : obj.genesis_state) {
    genesis_state[dev::toJS(k)] = enc_json(v);
  }
  return json;
}

void dec_json(Json::Value const& json, DPOSConfig& obj) {
  obj.eligibility_balance_threshold = dev::jsToU256(json["eligibility_balance_threshold"].asString());
  obj.deposit_delay = dev::jsToInt(json["deposit_delay"].asString());
  obj.withdrawal_delay = dev::jsToInt(json["withdrawal_delay"].asString());
  obj.vote_eligibility_balance_step = dev::jsToInt(json["vote_eligibility_balance_step"].asString());
  auto const& genesis_state = json["genesis_state"];
  for (auto const& k : genesis_state.getMemberNames()) {
    dec_json(genesis_state[k], obj.genesis_state[addr_t(k)]);
  }
}

Json::Value enc_json(ExecutionOptions const& obj) {
  Json::Value json(Json::objectValue);
  json["disable_nonce_check"] = obj.disable_nonce_check;
  json["disable_gas_fee"] = obj.disable_gas_fee;
  return json;
}

void dec_json(Json::Value const& json, ExecutionOptions& obj) {
  obj.disable_nonce_check = json["disable_nonce_check"].asBool();
  obj.disable_gas_fee = json["disable_gas_fee"].asBool();
}

RLP_FIELDS_DEFINE(ExecutionOptions, disable_nonce_check, disable_gas_fee)
RLP_FIELDS_DEFINE(ETHChainConfig, homestead_block, dao_fork_block, eip_150_block, eip_158_block, byzantium_block,
                  constantinople_block, petersburg_block)
RLP_FIELDS_DEFINE(DPOSConfig, eligibility_balance_threshold, vote_eligibility_balance_step, deposit_delay,
                  withdrawal_delay, genesis_state)
RLP_FIELDS_DEFINE(Config, eth_chain_config, disable_block_rewards, execution_options, genesis_balances, dpos, hardforks)
RLP_FIELDS_DEFINE(Opts, expected_max_trx_per_block, max_trie_full_node_levels_to_cache)
RLP_FIELDS_DEFINE(OptsDB, db_path, disable_most_recent_trie_value_views)

}  // namespace taraxa::state_api