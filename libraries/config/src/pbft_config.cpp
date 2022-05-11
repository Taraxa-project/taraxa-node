#include "config/pbft_config.hpp"

#include <libdevcore/CommonJS.h>

namespace taraxa {

Json::Value enc_json(PbftConfig const& obj) {
  Json::Value ret(Json::objectValue);
  ret["lambda_ms_min"] = dev::toJS(obj.lambda_ms_min);
  ret["committee_size"] = dev::toJS(obj.committee_size);
  ret["number_of_proposers"] = dev::toJS(obj.number_of_proposers);
  ret["dag_blocks_size"] = dev::toJS(obj.dag_blocks_size);
  ret["ghost_path_move_back"] = dev::toJS(obj.ghost_path_move_back);
  ret["run_count_votes"] = obj.run_count_votes;
  ret["gas_limit"] = dev::toJS(obj.gas_limit);
  return ret;
}

void dec_json(Json::Value const& json, PbftConfig& obj) {
  obj.lambda_ms_min = dev::jsToInt(json["lambda_ms_min"].asString());
  obj.committee_size = dev::jsToInt(json["committee_size"].asString());
  obj.number_of_proposers = dev::jsToInt(json["number_of_proposers"].asString());
  obj.dag_blocks_size = dev::jsToInt(json["dag_blocks_size"].asString());
  obj.ghost_path_move_back = dev::jsToInt(json["ghost_path_move_back"].asString());
  obj.run_count_votes = json["run_count_votes"].asBool();
  obj.gas_limit = dev::getUInt(json["gas_limit"]);
}

}  // namespace taraxa