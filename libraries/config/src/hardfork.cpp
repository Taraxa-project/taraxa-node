#include "config/hardfork.hpp"

Json::Value enc_json(const Hardforks& obj) {
  Json::Value json(Json::objectValue);

  auto& rewards = json["rewards_distribution_frequency"];
  rewards = Json::objectValue;
  for (auto i = obj.rewards_distribution_frequency.begin(); i != obj.rewards_distribution_frequency.end(); ++i) {
    rewards[std::to_string(i->first)] = i->second;
  }

  json["magnolia_hf_block_num"] = obj.magnolia_hf_block_num;

  return json;
}

void dec_json(const Json::Value& json, Hardforks& obj) {
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

RLP_FIELDS_DEFINE(Hardforks, rewards_distribution_frequency, magnolia_hf_block_num)
