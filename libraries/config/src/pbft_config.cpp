#include "config/pbft_config.hpp"

#include <libdevcore/CommonJS.h>
#include <libdevcore/RLP.h>

namespace taraxa {

Json::Value enc_json(PbftConfig const& obj) {
  Json::Value ret(Json::objectValue);
  ret["lambda_ms_min"] = dev::toJS(obj.lambda_ms_min);
  ret["committee_size"] = dev::toJS(obj.committee_size);
  ret["number_of_proposers"] = dev::toJS(obj.number_of_proposers);
  ret["gas_limit"] = dev::toJS(obj.gas_limit);
  return ret;
}

void dec_json(Json::Value const& json, PbftConfig& obj) {
  obj.lambda_ms_min = dev::jsToInt(json["lambda_ms_min"].asString());
  obj.committee_size = dev::jsToInt(json["committee_size"].asString());
  obj.number_of_proposers = dev::jsToInt(json["number_of_proposers"].asString());
  obj.gas_limit = dev::getUInt(json["gas_limit"]);
}

bytes PbftConfig::rlp() const {
  dev::RLPStream s;
  s.appendList(4);

  s << lambda_ms_min;
  s << committee_size;
  s << number_of_proposers;
  s << gas_limit;

  return s.out();
}

}  // namespace taraxa