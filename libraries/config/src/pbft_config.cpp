#include "config/pbft_config.hpp"

#include <libdevcore/CommonJS.h>

namespace taraxa {

Json::Value enc_json(PbftConfig const& obj) {
  Json::Value ret(Json::objectValue);
  ret["lambda_uint"] = dev::toJS(obj.lambda_uint);
  ret["committee_size"] = dev::toJS(obj.committee_size);
  ret["max_ghost_size"] = dev::toJS(obj.max_ghost_size);
  ret["ghost_path_move_back"] = dev::toJS(obj.ghost_path_move_back);
  ret["debug_count_votes"] = obj.debug_count_votes;
  return ret;
}

void dec_json(Json::Value const& json, PbftConfig& obj) {
  obj.lambda_uint = dev::jsToInt(json["lambda_uint"].asString());
  obj.committee_size = dev::jsToInt(json["committee_size"].asString());
  obj.max_ghost_size = dev::jsToInt(json["max_ghost_size"].asString());
  obj.ghost_path_move_back = dev::jsToInt(json["ghost_path_move_back"].asString());
  obj.debug_count_votes = json["debug_count_votes"].asBool();
}

}  // namespace taraxa