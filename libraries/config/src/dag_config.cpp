#include "config/dag_config.hpp"

#include <libdevcore/CommonJS.h>
#include <libdevcore/RLP.h>

namespace taraxa {

bytes DagConfig::rlp() const {
  dev::RLPStream s;
  s.appendList(1);

  s << gas_limit;

  return s.out();
}

Json::Value enc_json(const DagConfig& obj) {
  Json::Value ret(Json::objectValue);
  ret["gas_limit"] = dev::toJS(obj.gas_limit);
  return ret;
}

void dec_json(const Json::Value& json, DagConfig& obj) { obj.gas_limit = dev::getUInt(json["gas_limit"]); }

}  // namespace taraxa