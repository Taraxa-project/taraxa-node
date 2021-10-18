#include "vdf/config.hpp"

#include <libdevcore/CommonJS.h>

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

Json::Value enc_json(SortitionParams const& obj) {
  Json::Value ret(Json::objectValue);
  ret["vrf"] = enc_json(obj.vrf);
  ret["vdf"] = enc_json(obj.vdf);
  return ret;
}

void dec_json(Json::Value const& json, SortitionParams& obj) {
  dec_json(json["vrf"], obj.vrf);
  dec_json(json["vdf"], obj.vdf);
}

Json::Value enc_json(SortitionConfig const& obj) {
  auto ret = enc_json(dynamic_cast<SortitionParams const&>(obj));
  ret["changes_count_for_average"] = dev::toJS(obj.changes_count_for_average);
  ret["max_interval_correction"] = dev::toJS(obj.max_interval_correction);
  ret["target_dag_efficiency"] = dev::toJS(obj.target_dag_efficiency);
  ret["computation_interval"] = dev::toJS(obj.computation_interval);
  return ret;
}

void dec_json(Json::Value const& json, SortitionConfig& obj) {
  dec_json(json, dynamic_cast<SortitionParams&>(obj));
  obj.changes_count_for_average = dev::jsToInt(json["changes_count_for_average"].asString());
  obj.max_interval_correction = dev::jsToInt(json["max_interval_correction"].asString());
  obj.target_dag_efficiency = dev::jsToInt(json["target_dag_efficiency"].asString());
  obj.computation_interval = dev::jsToInt(json["computation_interval"].asString());
}

}  // namespace taraxa