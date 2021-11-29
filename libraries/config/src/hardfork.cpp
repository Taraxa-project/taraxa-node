#include "config/hardfork.hpp"

Json::Value enc_json(const Hardforks& obj) {
  Json::Value json(Json::objectValue);
  json["enable_vrf_adjustion_block"] = dev::toJS(obj.enable_vrf_adjustion_block);
  return json;
}

void dec_json(const Json::Value& json, Hardforks& obj) {
  if (auto const& e = json["enable_vrf_adjustion_block"]) {
    obj.enable_vrf_adjustion_block = dev::getUInt(e);
  }
}