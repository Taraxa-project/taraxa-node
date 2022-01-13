#include "config/hardfork.hpp"

Json::Value enc_json(const Hardforks& obj) {
  Json::Value json(Json::objectValue);
  json["fix_genesis_hardfork_block_num"] = dev::toJS(obj.fix_genesis_hardfork_block_num);
  return json;
}

void dec_json(const Json::Value& json, Hardforks& obj) {
  if (auto const& e = json["fix_genesis_hardfork_block_num"]) {
    obj.fix_genesis_hardfork_block_num = dev::getUInt(e);
  }
}

RLP_FIELDS_DEFINE(Hardforks, fix_genesis_hardfork_block_num)
