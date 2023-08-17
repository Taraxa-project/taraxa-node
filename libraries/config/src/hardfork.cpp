#include "config/hardfork.hpp"

Json::Value enc_json(const Redelegation& obj) {
  Json::Value json(Json::objectValue);
  json["validator"] = dev::toJS(obj.validator);
  json["delegator"] = dev::toJS(obj.delegator);
  json["amount"] = dev::toJS(obj.amount);
  return json;
}

void dec_json(const Json::Value& json, Redelegation& obj) {
  obj.validator = taraxa::addr_t(json["validator"].asString());
  obj.delegator = taraxa::addr_t(json["delegator"].asString());
  obj.amount = dev::jsToU256(json["amount"].asString());
}

RLP_FIELDS_DEFINE(Redelegation, validator, delegator, amount)

Json::Value enc_json(const HardforksConfig& obj) {
  Json::Value json(Json::objectValue);
  json["fix_redelegate_block_num"] = dev::toJS(obj.fix_redelegate_block_num);
  json["initial_validators"] = Json::Value(Json::arrayValue);
  for (const auto& v : obj.redelegations) {
    json["redelegations"].append(enc_json(v));
  }

  auto& rewards = json["rewards_distribution_frequency"];
  rewards = Json::objectValue;
  for (auto i = obj.rewards_distribution_frequency.begin(); i != obj.rewards_distribution_frequency.end(); ++i) {
    rewards[std::to_string(i->first)] = i->second;
  }

  json["magnolia_hf_block_num"] = obj.magnolia_hf_block_num;

  return json;
}

void dec_json(const Json::Value& json, HardforksConfig& obj) {
  obj.fix_redelegate_block_num = dev::getUInt(json["fix_redelegate_block_num"]);

  const auto& redelegations_json = json["redelegations"];
  obj.redelegations = std::vector<Redelegation>(redelegations_json.size());

  for (uint32_t i = 0; i < redelegations_json.size(); ++i) {
    dec_json(redelegations_json[i], obj.redelegations[i]);
  }

  if (const auto& e = json["rewards_distribution_frequency"]) {
    assert(e.isObject());

    for (auto itr = e.begin(); itr != e.end(); ++itr) {
      obj.rewards_distribution_frequency[dev::getUInt(itr.key())] = dev::getUInt(*itr);
    }
  }
  if (const auto& e = json["magnolia_hf_block_num"]) {
    obj.magnolia_hf_block_num = dev::getUInt(e);
  }
}

RLP_FIELDS_DEFINE(HardforksConfig, fix_redelegate_block_num, redelegations, rewards_distribution_frequency,
                  magnolia_hf_block_num)
