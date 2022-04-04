#include "config/dag_config.hpp"

#include <libdevcore/CommonJS.h>

namespace taraxa {

Json::Value enc_json(const DagConfig& obj) {
  Json::Value ret(Json::objectValue);
  ret["gas_limit"] = dev::toJS(obj.gas_limit);
  return ret;
}

void dec_json(const Json::Value& json, DagConfig& obj) { obj.gas_limit = dev::jsToU256(json["gas_limit"].asString()); }

}  // namespace taraxa