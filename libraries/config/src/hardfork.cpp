#include "config/hardfork.hpp"

Json::Value enc_json(const Hardforks& obj) {
  Json::Value json(Json::objectValue);
  json["fix_genesis_fork_block"] = dev::toJS(obj.fix_genesis_fork_block);
  return json;
}

void dec_json(const Json::Value& json, Hardforks& obj) {
  if (auto const& e = json["fix_genesis_fork_block"]) {
    obj.fix_genesis_fork_block = dev::getUInt(e);
  }
}

RLP_FIELDS_DEFINE(Hardforks, fix_genesis_fork_block)
