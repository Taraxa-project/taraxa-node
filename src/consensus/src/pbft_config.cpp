#include <libdevcore/CommonJS.h>

#include <consensus/pbft_config.hpp>

namespace taraxa {

Json::Value enc_json(PbftConfig const& obj) {
  Json::Value ret(Json::objectValue);
  ret["lambda_ms_min"] = dev::toJS(obj.lambda_ms_min);
  ret["committee_size"] = dev::toJS(obj.committee_size);
  ret["dag_blocks_size"] = dev::toJS(obj.dag_blocks_size);
  ret["ghost_path_move_back"] = dev::toJS(obj.ghost_path_move_back);
  ret["run_count_votes"] = obj.run_count_votes;
  return ret;
}

void dec_json(Json::Value const& json, PbftConfig& obj) {
  obj.lambda_ms_min = dev::jsToInt(json["lambda_ms_min"].asString());
  obj.committee_size = dev::jsToInt(json["committee_size"].asString());
  obj.dag_blocks_size = dev::jsToInt(json["dag_blocks_size"].asString());
  obj.ghost_path_move_back = dev::jsToInt(json["ghost_path_move_back"].asString());
  obj.run_count_votes = json["run_count_votes"].asBool();
}

}  // namespace taraxa