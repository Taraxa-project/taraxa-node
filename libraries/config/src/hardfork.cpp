#include "config/hardfork.hpp"

#include "common/config_exception.hpp"

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
  obj.block_num = json["block_num"].isUInt64() ? dev::getUInt(json["block_num"]) : uint64_t(-1);
  obj.jail_time = dev::getUInt(json["jail_time"]);
}
RLP_FIELDS_DEFINE(MagnoliaHardfork, block_num, jail_time)

Json::Value enc_json(const AspenHardfork& obj) {
  Json::Value json(Json::objectValue);
  json["block_num_part_one"] = dev::toJS(obj.block_num_part_one);
  json["block_num_part_two"] = dev::toJS(obj.block_num_part_two);
  json["max_supply"] = dev::toJS(obj.max_supply);
  json["generated_rewards"] = dev::toJS(obj.generated_rewards);
  return json;
}

void dec_json(const Json::Value& json, AspenHardfork& obj) {
  obj.block_num_part_one =
      json["block_num_part_one"].isUInt64() ? dev::getUInt(json["block_num_part_one"]) : uint64_t(-1);
  obj.block_num_part_two =
      json["block_num_part_two"].isUInt64() ? dev::getUInt(json["block_num_part_two"]) : uint64_t(-1);
  obj.max_supply = dev::jsToU256(json["max_supply"].asString());
  obj.generated_rewards = dev::jsToU256(json["generated_rewards"].asString());
}
RLP_FIELDS_DEFINE(AspenHardfork, block_num_part_one, block_num_part_two, max_supply, generated_rewards)

bool FicusHardforkConfig::isFicusHardfork(taraxa::PbftPeriod period) const { return period >= block_num; }

bool FicusHardforkConfig::isPillarBlockPeriod(taraxa::PbftPeriod period, bool skip_first_pillar_block) const {
  return period >= block_num &&
         period >= firstPillarBlockPeriod() + (skip_first_pillar_block ? 1 : 0) * pillar_blocks_interval &&
         period % pillar_blocks_interval == 0;
}

bool FicusHardforkConfig::isPbftWithPillarBlockPeriod(taraxa::PbftPeriod period) const {
  return period >= firstPillarBlockPeriod() && period % pillar_blocks_interval == pbft_inclusion_delay;
}

// Returns first pillar block period
taraxa::PbftPeriod FicusHardforkConfig::firstPillarBlockPeriod() const {
  return block_num ? block_num : pillar_blocks_interval;
}

void FicusHardforkConfig::validate(uint32_t delegation_delay) const {
  // Ficus hardfork is disabled - do not validate configuration
  if (block_num == uint64_t(-1)) {
    return;
  }

  if (block_num % pillar_blocks_interval) {
    throw taraxa::ConfigException("ficus_hf.block_num % ficus_hf.pillar_blocks_interval must == 0");
  }

  if (pillar_blocks_interval <= pillar_chain_sync_interval) {
    throw taraxa::ConfigException("ficus_hf.pillar_blocks_interval must be > ficus_hf.pillar_chain_sync_interval");
  }

  if (pillar_chain_sync_interval <= pbft_inclusion_delay) {
    throw taraxa::ConfigException("ficus_hf.pillar_chain_sync_interval must be > ficus_hf.pbft_inclusion_delay");
  }

  if (pbft_inclusion_delay < 1 || pbft_inclusion_delay <= delegation_delay) {
    throw taraxa::ConfigException("ficus_hf.pbft_inclusion_delay must be >= 1 && > dpos.delegation_delay");
  }
}

Json::Value enc_json(const FicusHardforkConfig& obj) {
  Json::Value json(Json::objectValue);
  json["block_num"] = dev::toJS(obj.block_num);
  json["pillar_blocks_interval"] = dev::toJS(obj.pillar_blocks_interval);
  json["pillar_chain_sync_interval"] = dev::toJS(obj.pillar_chain_sync_interval);
  json["pbft_inclusion_delay"] = dev::toJS(obj.pbft_inclusion_delay);
  json["bridge_contract_address"] = dev::toJS(obj.bridge_contract_address);
  return json;
}

void dec_json(const Json::Value& json, FicusHardforkConfig& obj) {
  obj.block_num = json["block_num"].isUInt64() ? dev::getUInt(json["block_num"]) : uint64_t(-1);
  obj.pillar_blocks_interval = dev::getUInt(json["pillar_blocks_interval"]);
  obj.pillar_chain_sync_interval = dev::getUInt(json["pillar_chain_sync_interval"]);
  obj.pbft_inclusion_delay = dev::getUInt(json["pbft_inclusion_delay"]);
  obj.bridge_contract_address = taraxa::addr_t(json["bridge_contract_address"].asString());
}

RLP_FIELDS_DEFINE(FicusHardforkConfig, block_num, pillar_blocks_interval, pillar_chain_sync_interval,
                  pbft_inclusion_delay, bridge_contract_address)

// Json::Value enc_json(const BambooRedelegation& obj) {
//   Json::Value json(Json::objectValue);
//   json["validator"] = dev::toJS(obj.validator);
//   json["amount"] = dev::toJS(obj.amount);
//   return json;
// }

// void dec_json(const Json::Value& json, BambooRedelegation& obj) {
//   obj.validator = taraxa::addr_t(json["validator"].asString());
//   obj.amount = dev::jsToU256(json["amount"].asString());
// }

// RLP_FIELDS_DEFINE(BambooRedelegation, validator, amount)

// Json::Value enc_json(const BambooHardfork& obj) {
//   Json::Value json(Json::objectValue);
//   json["block_num"] = dev::toJS(obj.block_num);
//   for (const auto& v : obj.redelegations) {
//     json["redelegations"].append(enc_json(v));
//   }
//   return json;
// }

// void dec_json(const Json::Value& json, BambooHardfork& obj) {
//   obj.block_num = json["block_num"].isUInt64() ? dev::getUInt(json["block_num"]) : uint64_t(-1);

//   const auto& redelegations_json = json["redelegations"];
//   obj.redelegations = std::vector<BambooRedelegation>(redelegations_json.size());
//   for (uint32_t i = 0; i < redelegations_json.size(); ++i) {
//     dec_json(redelegations_json[i], obj.redelegations[i]);
//   }
// }
// RLP_FIELDS_DEFINE(BambooHardfork, block_num, redelegations)

Json::Value enc_json(const HardforksConfig& obj) {
  Json::Value json(Json::objectValue);
  json["fix_redelegate_block_num"] = dev::toJS(obj.fix_redelegate_block_num);
  json["fix_claim_all_block_num"] = dev::toJS(obj.fix_claim_all_block_num);
  json["phalaenopsis_hf_block_num"] = dev::toJS(obj.phalaenopsis_hf_block_num);
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
  json["aspen_hf"] = enc_json(obj.aspen_hf);
  json["ficus_hf"] = enc_json(obj.ficus_hf);
  // json["bamboo_hf"] = enc_json(obj.bamboo_hf);

  return json;
}

void dec_json(const Json::Value& json, HardforksConfig& obj) {
  obj.fix_redelegate_block_num =
      json["fix_redelegate_block_num"].isUInt64() ? dev::getUInt(json["fix_redelegate_block_num"]) : uint64_t(-1);
  obj.fix_claim_all_block_num =
      json["fix_claim_all_block_num"].isUInt64() ? dev::getUInt(json["fix_claim_all_block_num"]) : uint64_t(-1);
  obj.phalaenopsis_hf_block_num =
      json["phalaenopsis_hf_block_num"].isUInt64() ? dev::getUInt(json["phalaenopsis_hf_block_num"]) : uint64_t(-1);

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

  dec_json(json["magnolia_hf"], obj.magnolia_hf);
  dec_json(json["aspen_hf"], obj.aspen_hf);
  dec_json(json["ficus_hf"], obj.ficus_hf);
  // dec_json(json["bamboo_hf"], obj.bamboo_hf);
}

RLP_FIELDS_DEFINE(HardforksConfig, fix_redelegate_block_num, redelegations, rewards_distribution_frequency, magnolia_hf,
                  phalaenopsis_hf_block_num, fix_claim_all_block_num, aspen_hf, ficus_hf)