#include "config/hardfork.hpp"

Json::Value enc_json(const RedelegationMap& obj) {
  Json::Value json(Json::objectValue);
  for (auto const& [k, v] : obj) {
    json[dev::toJS(k)] = dev::toJS(v);
  }
  return json;
}

void dec_json(const Json::Value& json, RedelegationMap& obj) {
  for (const auto& k : json.getMemberNames()) {
    obj[taraxa::addr_t(k)] = taraxa::addr_t(json[k].asString());
  }
}

Json::Value enc_json(const Hardforks& obj) {
  Json::Value json(Json::objectValue);
  json["fix_redelegate_block_num"] = dev::toJS(obj.fix_redelegate_block_num);
  json["redelegations"] = enc_json(obj.redelegations);
  return json;
}

void dec_json(const Json::Value& json, Hardforks& obj) {
  obj.fix_redelegate_block_num = dev::getUInt(json["fix_redelegate_block_num"]);
  dec_json(json["redelegations"], obj.redelegations);
}

RLP_FIELDS_DEFINE(Hardforks, fix_redelegate_block_num, redelegations)