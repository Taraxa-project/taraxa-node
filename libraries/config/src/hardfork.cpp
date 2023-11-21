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

Json::Value enc_json(const MagnoliaHardfork& obj) {
  Json::Value json(Json::objectValue);
  json["block_num"] = dev::toJS(obj.block_num);
  json["jail_time"] = dev::toJS(obj.jail_time);
  return json;
}

void dec_json(const Json::Value& json, MagnoliaHardfork& obj) {
  obj.block_num = dev::getUInt(json["block_num"]);
  obj.jail_time = dev::getUInt(json["jail_time"]);
}
RLP_FIELDS_DEFINE(MagnoliaHardfork, block_num, jail_time)

Json::Value enc_json(const FicusHardfork& obj) {
  Json::Value json(Json::objectValue);
  json["block_num"] = dev::toJS(obj.block_num);
  json["pillar_block_periods"] = dev::toJS(obj.pillar_block_periods);
  return json;
}

void dec_json(const Json::Value& json, FicusHardfork& obj) {
  obj.block_num = json["block_num"].isUInt64() ? dev::getUInt(json["block_num"]) : uint64_t(-1);
  obj.pillar_block_periods = dev::getUInt(json["pillar_block_periods"]);
}
RLP_FIELDS_DEFINE(FicusHardfork, block_num, pillar_block_periods)

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

  json["magnolia_hf"] = enc_json(obj.magnolia_hf);
  json["ficus_hf"] = enc_json(obj.ficus_hf);

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
  if (const auto& e = json["magnolia_hf"]) {
    dec_json(e, obj.magnolia_hf);
  }

  dec_json(json["ficus_hf"], obj.ficus_hf);
}

RLP_FIELDS_DEFINE(HardforksConfig, fix_redelegate_block_num, redelegations, rewards_distribution_frequency, magnolia_hf,
                  ficus_hf)
