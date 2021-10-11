#include <libdevcore/CommonJS.h>

#include <vdf/config.hpp>

namespace taraxa {

Json::Value enc_json(VrfParams const& obj) {
  Json::Value ret(Json::objectValue);
  ret["threshold_upper"] = dev::toJS(obj.threshold_upper);
  ret["threshold_lower"] = dev::toJS(obj.threshold_lower);
  return ret;
}
void dec_json(Json::Value const& json, VrfParams& obj) {
  obj.threshold_upper = dev::jsToInt(json["threshold_upper"].asString());
  obj.threshold_lower = dev::jsToInt(json["threshold_lower"].asString());
}

Json::Value enc_json(VdfParams const& obj) {
  Json::Value ret(Json::objectValue);
  ret["difficulty_min"] = dev::toJS(obj.difficulty_min);
  ret["difficulty_max"] = dev::toJS(obj.difficulty_max);
  ret["difficulty_stale"] = dev::toJS(obj.difficulty_stale);
  ret["lambda_bound"] = dev::toJS(obj.lambda_bound);
  return ret;
}

void dec_json(Json::Value const& json, VdfParams& obj) {
  obj.difficulty_min = dev::jsToInt(json["difficulty_min"].asString());
  obj.difficulty_max = dev::jsToInt(json["difficulty_max"].asString());
  obj.difficulty_stale = dev::jsToInt(json["difficulty_stale"].asString());
  obj.lambda_bound = dev::jsToInt(json["lambda_bound"].asString());
}

Json::Value enc_json(SortitionConfig const& obj) {
  Json::Value ret(Json::objectValue);
  ret["vrf"] = enc_json(obj.vrf);
  ret["vdf"] = enc_json(obj.vdf);
  return ret;
}

void dec_json(Json::Value const& json, SortitionConfig& obj) {
  dec_json(json["vrf"], obj.vrf);
  dec_json(json["vdf"], obj.vdf);
}

}  // namespace taraxa