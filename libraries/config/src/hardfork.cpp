#include "config/hardfork.hpp"

#include "common/config_exception.hpp"

namespace taraxa {
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
  // Pillar block hash is included in the next pbft block with period +1
  return period >= firstPillarBlockPeriod() && period % pillar_blocks_interval == 1;
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

  if (block_num < delegation_delay) {
    throw taraxa::ConfigException("ficus_hf.block_num must be >= dpos.delegation_delay");
  }

  if (pillar_blocks_interval <= 1) {
    throw taraxa::ConfigException("ficus_hf.pillar_blocks_interval must be > 1");
  }

  if (block_num % pillar_blocks_interval) {
    throw taraxa::ConfigException("ficus_hf.block_num % ficus_hf.pillar_blocks_interval must == 0");
  }
}

Json::Value enc_json(const FicusHardforkConfig& obj) {
  Json::Value json(Json::objectValue);
  json["block_num"] = dev::toJS(obj.block_num);
  json["pillar_blocks_interval"] = dev::toJS(obj.pillar_blocks_interval);
  json["bridge_contract_address"] = dev::toJS(obj.bridge_contract_address);
  return json;
}

void dec_json(const Json::Value& json, FicusHardforkConfig& obj) {
  obj.block_num = json["block_num"].isUInt64() ? dev::getUInt(json["block_num"]) : uint64_t(-1);
  obj.pillar_blocks_interval = dev::getUInt(json["pillar_blocks_interval"]);
  obj.bridge_contract_address = taraxa::addr_t(json["bridge_contract_address"].asString());
}

RLP_FIELDS_DEFINE(FicusHardforkConfig, block_num, pillar_blocks_interval, bridge_contract_address)

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

Json::Value enc_json(const CornusHardforkConfig& obj) {
  Json::Value json(Json::objectValue);
  json["block_num"] = dev::toJS(obj.block_num);
  json["delegation_locking_period"] = dev::toJS(obj.delegation_locking_period);
  json["dag_gas_limit"] = dev::toJS(obj.dag_gas_limit);
  json["pbft_gas_limit"] = dev::toJS(obj.pbft_gas_limit);
  return json;
}

void dec_json(const Json::Value& json, CornusHardforkConfig& obj) {
  obj.block_num = json["block_num"].isUInt64() ? dev::getUInt(json["block_num"]) : uint64_t(-1);
  obj.delegation_locking_period = dev::getUInt(json["delegation_locking_period"]);
  obj.dag_gas_limit = dev::getUInt(json["dag_gas_limit"]);
  obj.pbft_gas_limit = dev::getUInt(json["pbft_gas_limit"]);
}

RLP_FIELDS_DEFINE(CornusHardforkConfig, block_num, delegation_locking_period, dag_gas_limit, pbft_gas_limit)

Json::Value enc_json(const SoleiroliaHardforkConfig& obj) {
  Json::Value json(Json::objectValue);
  json["block_num"] = dev::toJS(obj.block_num);
  json["trx_min_gas_price"] = dev::toJS(obj.trx_min_gas_price);
  json["trx_max_gas_limit"] = dev::toJS(obj.trx_max_gas_limit);
  return json;
}

void dec_json(const Json::Value& json, SoleiroliaHardforkConfig& obj) {
  obj.block_num = json["block_num"].isUInt64() ? dev::getUInt(json["block_num"]) : uint64_t(-1);
  obj.trx_min_gas_price = dev::getUInt(json["trx_min_gas_price"]);
  obj.trx_max_gas_limit = dev::getUInt(json["trx_max_gas_limit"]);
}

RLP_FIELDS_DEFINE(SoleiroliaHardforkConfig, block_num, trx_min_gas_price, trx_max_gas_limit)

Json::Value enc_json(const CactiHardforkConfig& obj) {
  Json::Value json(Json::objectValue);
  json["block_num"] = dev::toJS(obj.block_num);
  json["lambda_min"] = dev::toJS(obj.lambda_min);
  json["lambda_max"] = dev::toJS(obj.lambda_max);
  json["lambda_default"] = dev::toJS(obj.lambda_default);
  json["lambda_change_interval"] = dev::toJS(obj.lambda_change_interval);
  json["lambda_change"] = dev::toJS(obj.lambda_change);
  json["block_propagation_min"] = dev::toJS(obj.block_propagation_min);
  json["block_propagation_max"] = dev::toJS(obj.block_propagation_max);
  json["consensus_delay"] = dev::toJS(obj.consensus_delay);
  json["delegation_locking_period"] = dev::toJS(obj.delegation_locking_period);
  json["jail_time"] = dev::toJS(obj.jail_time);

  Json::Value targets(Json::arrayValue);
  targets.append(obj.dag_efficiency_targets.first);
  targets.append(obj.dag_efficiency_targets.second);
  json["dag_efficiency_targets"] = targets;

  return json;
}

void dec_json(const Json::Value& json, CactiHardforkConfig& obj) {
  obj.block_num = json["block_num"].isUInt64() ? dev::getUInt(json["block_num"]) : uint64_t(-1);
  obj.lambda_min = dev::getUInt(json["lambda_min"]);
  obj.lambda_max = dev::getUInt(json["lambda_max"]);
  obj.lambda_default = dev::getUInt(json["lambda_default"]);
  obj.lambda_change_interval = dev::getUInt(json["lambda_change_interval"]);
  obj.lambda_change = dev::getUInt(json["lambda_change"]);
  obj.block_propagation_min = dev::getUInt(json["block_propagation_min"]);
  obj.block_propagation_max = dev::getUInt(json["block_propagation_max"]);
  obj.consensus_delay = dev::getUInt(json["consensus_delay"]);
  obj.delegation_locking_period = dev::getUInt(json["delegation_locking_period"]);
  obj.jail_time = dev::getUInt(json["jail_time"]);

  auto first = json["dag_efficiency_targets"][0].asInt();
  auto second = json["dag_efficiency_targets"][1].asInt();
  obj.dag_efficiency_targets = {first, second};
}

void CactiHardforkConfig::validate(uint32_t rewards_distribution_frequency) const {
  // Do not validate configuration in case it is disabled or enabled from block 1
  if (block_num == uint64_t(-1) || block_num <= 1) {
    return;
  }

  // No need to validate cacti hf block num in case of rewards distribution frequency == 1
  if (rewards_distribution_frequency == 1) {
    return;
  }

  // cacti_hf.block_num must be equal to rewards_distribution_frequency + 1, otherwise we would have to do a db
  // migration for DbStorage::Columns::block_rewards_stats db column as cacti hardfork added new item into the rlp saved
  // in that column. It is cleader every rewards_distribution_frequency blocks, thus if cacti hf block num is + 1, no
  // migration is needed
  if (block_num % rewards_distribution_frequency != 1) {
    throw taraxa::ConfigException("cacti_hf.block_num must be equal to rewards_distribution_frequency + 1");
  }

  return;
}

RLP_FIELDS_DEFINE(CactiHardforkConfig, block_num, lambda_min, lambda_max, lambda_default, lambda_change_interval,
                  lambda_change, block_propagation_min, block_propagation_max, consensus_delay,
                  delegation_locking_period, jail_time)

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
  json["cornus_hf"] = enc_json(obj.cornus_hf);
  json["soleirolia_hf"] = enc_json(obj.soleirolia_hf);
  json["cacti_hf"] = enc_json(obj.cacti_hf);

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
  dec_json(json["cornus_hf"], obj.cornus_hf);
  dec_json(json["soleirolia_hf"], obj.soleirolia_hf);
  dec_json(json["cacti_hf"], obj.cacti_hf);
}

uint32_t HardforksConfig::getRewardsDistributionFrequency(uint64_t block) const {
  const auto& distribution_frequencies = rewards_distribution_frequency;
  auto itr = distribution_frequencies.upper_bound(block);
  if (distribution_frequencies.empty() || itr == distribution_frequencies.begin()) {
    return 1;
  }
  return (--itr)->second;
}

RLP_FIELDS_DEFINE(HardforksConfig, fix_redelegate_block_num, redelegations, rewards_distribution_frequency, magnolia_hf,
                  phalaenopsis_hf_block_num, fix_claim_all_block_num, aspen_hf, ficus_hf, cornus_hf, soleirolia_hf,
                  cacti_hf)
}  // namespace taraxa