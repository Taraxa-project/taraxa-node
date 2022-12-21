#include "config/state_config.hpp"

#include <libdevcore/CommonJS.h>

#include <sstream>

#include "common/vrf_wrapper.hpp"

namespace taraxa::state_api {

Json::Value enc_json(const EVMChainConfig& /*obj*/) {
  Json::Value json(Json::objectValue);
  // json["homestead_block"] = dev::toJS(obj.homestead_block);
  return json;
}

void dec_json(const Json::Value& /*json*/, EVMChainConfig& /*obj*/) {
  //   obj.homestead_block = dev::jsToInt(json["homestead_block"].asString());
}

void append_json(Json::Value& json, const Config& obj) {
  json["evm_chain_config"] = enc_json(obj.evm_chain_config);
  json["initial_balances"] = enc_json(obj.initial_balances);
  // json["hardforks"] = enc_json(obj.hardforks);
  json["dpos"] = enc_json(obj.dpos);
}

void dec_json(const Json::Value& json, Config& obj) {
  dec_json(json["evm_chain_config"], obj.evm_chain_config);
  dec_json(json["initial_balances"], obj.initial_balances);
  // dec_json(json["hardforks"], obj.hardforks);
  dec_json(json["dpos"], obj.dpos);
}

Json::Value enc_json(const BalanceMap& obj) {
  Json::Value json(Json::objectValue);
  for (auto const& [k, v] : obj) {
    json[dev::toJS(k)] = dev::toJS(v);
  }
  return json;
}

void dec_json(const Json::Value& json, BalanceMap& obj) {
  for (const auto& k : json.getMemberNames()) {
    obj[addr_t(k)] = dev::jsToU256(json[k].asString());
  }
}

Json::Value enc_json(const ValidatorInfo& obj) {
  Json::Value json(Json::objectValue);

  json["address"] = dev::toJS(obj.address);
  json["owner"] = dev::toJS(obj.owner);
  json["vrf_key"] = dev::toJS(obj.vrf_key);
  json["commission"] = dev::toJS(obj.commission);
  json["endpoint"] = obj.endpoint;
  json["description"] = obj.description;
  json["delegations"] = enc_json(obj.delegations);

  return json;
}
void dec_json(const Json::Value& json, ValidatorInfo& obj) {
  obj.address = addr_t(json["address"].asString());
  obj.owner = addr_t(json["owner"].asString());
  obj.vrf_key = vrf_wrapper::vrf_pk_t(json["vrf_key"].asString());
  obj.commission = dev::getUInt(json["commission"]);
  obj.endpoint = json["endpoint"].asString();
  obj.description = json["description"].asString();

  dec_json(json["delegations"], obj.delegations);
}

Json::Value enc_json(const DPOSConfig& obj) {
  Json::Value json(Json::objectValue);
  json["eligibility_balance_threshold"] = dev::toJS(obj.eligibility_balance_threshold);
  json["delegation_delay"] = dev::toJS(obj.delegation_delay);
  json["delegation_locking_period"] = dev::toJS(obj.delegation_locking_period);
  json["vote_eligibility_balance_step"] = dev::toJS(obj.vote_eligibility_balance_step);
  json["validator_maximum_stake"] = dev::toJS(obj.validator_maximum_stake);
  json["minimum_deposit"] = dev::toJS(obj.minimum_deposit);
  json["commission_change_delta"] = dev::toJS(obj.commission_change_delta);
  json["commission_change_frequency"] = dev::toJS(obj.commission_change_frequency);
  json["max_block_author_reward"] = dev::toJS(obj.max_block_author_reward);
  json["dag_proposers_reward"] = dev::toJS(obj.dag_proposers_reward);
  json["yield_percentage"] = dev::toJS(obj.yield_percentage);
  json["blocks_per_year"] = dev::toJS(obj.blocks_per_year);

  json["initial_validators"] = Json::Value(Json::arrayValue);
  for (const auto& v : obj.initial_validators) {
    json["initial_validators"].append(enc_json(v));
  }
  return json;
}

void dec_json(const Json::Value& json, DPOSConfig& obj) {
  obj.eligibility_balance_threshold = dev::jsToU256(json["eligibility_balance_threshold"].asString());
  obj.delegation_delay = dev::getUInt(json["delegation_delay"].asString());
  obj.delegation_locking_period = dev::getUInt(json["delegation_locking_period"].asString());
  obj.vote_eligibility_balance_step = dev::jsToU256(json["vote_eligibility_balance_step"].asString());
  obj.validator_maximum_stake = dev::jsToU256(json["validator_maximum_stake"].asString());
  obj.minimum_deposit = dev::jsToU256(json["minimum_deposit"].asString());
  obj.commission_change_delta = static_cast<uint16_t>(dev::getUInt(json["commission_change_delta"].asString()));
  obj.commission_change_frequency = dev::getUInt(json["commission_change_frequency"].asString());
  obj.max_block_author_reward = static_cast<uint16_t>(dev::getUInt(json["max_block_author_reward"].asString()));
  obj.dag_proposers_reward = static_cast<uint16_t>(dev::getUInt(json["dag_proposers_reward"].asString()));
  obj.yield_percentage = static_cast<uint16_t>(dev::getUInt(json["yield_percentage"]));
  obj.blocks_per_year = dev::getUInt(json["blocks_per_year"]);

  const auto& initial_validators_json = json["initial_validators"];
  obj.initial_validators = std::vector<ValidatorInfo>(initial_validators_json.size());

  for (uint32_t i = 0; i < initial_validators_json.size(); ++i) {
    dec_json(initial_validators_json[i], obj.initial_validators[i]);
  }
}

RLP_FIELDS_DEFINE(EVMChainConfig, chain_id)
RLP_FIELDS_DEFINE(ValidatorInfo, address, owner, vrf_key, commission, endpoint, description, delegations)
RLP_FIELDS_DEFINE(DPOSConfig, eligibility_balance_threshold, vote_eligibility_balance_step, validator_maximum_stake,
                  minimum_deposit, max_block_author_reward, dag_proposers_reward, commission_change_delta,
                  commission_change_frequency, delegation_delay, delegation_locking_period, blocks_per_year,
                  yield_percentage, initial_validators)
RLP_FIELDS_DEFINE(Config, evm_chain_config, initial_balances, dpos)
RLP_FIELDS_DEFINE(Opts, expected_max_trx_per_block, max_trie_full_node_levels_to_cache)
RLP_FIELDS_DEFINE(OptsDB, db_path, disable_most_recent_trie_value_views)

}  // namespace taraxa::state_api