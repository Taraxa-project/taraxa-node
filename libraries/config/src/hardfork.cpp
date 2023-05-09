#include "config/hardfork.hpp"

Json::Value enc_json(const Hardforks& obj) {
  Json::Value json(Json::objectValue);

  auto& rewards = json["rewards_distribution_frequency"];
  rewards = Json::objectValue;
  for (auto i = obj.rewards_distribution_frequency.begin(); i != obj.rewards_distribution_frequency.end(); ++i) {
    rewards[std::to_string(i->first)] = i->second;
  }
  return json;
}

void dec_json(const Json::Value& json, Hardforks& obj) {
  if (const auto& e = json["rewards_distribution_frequency"]) {
    assert(e.isObject());

    for (auto itr = e.begin(); itr != e.end(); ++itr) {
      obj.rewards_distribution_frequency[itr.key().asUInt64()] = itr->asUInt64();
    }
  }
}

RLP_FIELDS_DEFINE(Hardforks, rewards_distribution_frequency)
