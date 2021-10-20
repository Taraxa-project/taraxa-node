#include <libdevcore/CommonJS.h>

#include <vdf/config.hpp>

namespace taraxa {

Json::Value enc_json(VdfConfig const& obj) {
  Json::Value ret(Json::objectValue);
  ret["threshold_selection"] = dev::toJS(obj.threshold_selection);
  ret["threshold_vdf_omit"] = dev::toJS(obj.threshold_vdf_omit);
  ret["difficulty_min"] = dev::toJS(obj.difficulty_min);
  ret["difficulty_max"] = dev::toJS(obj.difficulty_max);
  ret["difficulty_stale"] = dev::toJS(obj.difficulty_stale);
  ret["lambda_bound"] = dev::toJS(obj.lambda_bound);
  return ret;
}

void dec_json(Json::Value const& json, VdfConfig& obj) {
  obj.threshold_selection = dev::jsToInt(json["threshold_selection"].asString());
  obj.threshold_vdf_omit = dev::jsToInt(json["threshold_vdf_omit"].asString());
  obj.difficulty_min = dev::jsToInt(json["difficulty_min"].asString());
  obj.difficulty_max = dev::jsToInt(json["difficulty_max"].asString());
  obj.difficulty_stale = dev::jsToInt(json["difficulty_stale"].asString());
  obj.lambda_bound = dev::jsToInt(json["lambda_bound"].asString());
}

}  // namespace taraxa