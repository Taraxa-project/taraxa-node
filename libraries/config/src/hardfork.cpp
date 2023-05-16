#include "config/hardfork.hpp"

Json::Value enc_json(const Hardforks& obj) {
  Json::Value json(Json::objectValue);
  json["fix_redelegate_block_num"] = dev::toJS(obj.fix_redelegate_block_num);
  return json;
}

void dec_json(const Json::Value& json, Hardforks& obj) {
  obj.fix_redelegate_block_num = dev::getUInt(json["fix_redelegate_block_num"]);
}

RLP_FIELDS_DEFINE(Hardforks, fix_redelegate_block_num)
