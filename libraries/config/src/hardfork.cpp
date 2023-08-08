#include "config/hardfork.hpp"

Json::Value enc_json(const Hardforks& obj) {
  Json::Value json(Json::objectValue);
  json["fix_state_after_redelegate"] = dev::toJS(obj.fix_state_after_redelegate);

  return json;
}

void dec_json(const Json::Value& json, Hardforks& obj) {
  obj.fix_state_after_redelegate = dev::getUInt(json["fix_state_after_redelegate"]);
}

RLP_FIELDS_DEFINE(Hardforks, fix_state_after_redelegate)