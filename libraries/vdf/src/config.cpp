#include "vdf/config.hpp"

#include <libdevcore/CommonJS.h>

namespace taraxa {

int32_t fixFromOverflow(uint16_t value, int32_t change, uint16_t limit) {
  int32_t diff = limit - value;
  if (std::abs(change) > std::abs(diff)) {
    return diff;
  }
  return change;
}

VrfParams& VrfParams::operator+=(int32_t change) {
  if (change < 0) {
    change = fixFromOverflow(threshold_upper, change, kThresholdUpperMinValue);
  } else {
    change = fixFromOverflow(threshold_upper, change, std::numeric_limits<uint16_t>::max());
  }
  threshold_upper += change;
  return *this;
}

Json::Value enc_json(VrfParams const& obj) {
  Json::Value ret(Json::objectValue);
  ret["threshold_upper"] = dev::toJS(obj.threshold_upper);
  ret["threshold_range"] = dev::toJS(obj.threshold_range);
  return ret;
}
void dec_json(Json::Value const& json, VrfParams& obj) {
  obj.threshold_upper = dev::jsToInt(json["threshold_upper"].asString());
  obj.threshold_range = dev::jsToInt(json["threshold_range"].asString());
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

  Json::Value targets(Json::arrayValue);
  targets.append(dev::toJS(obj.dag_efficiency_targets.first));
  targets.append(dev::toJS(obj.dag_efficiency_targets.second));

  ret["dag_efficiency_targets"] = targets;
  ret["computation_interval"] = dev::toJS(obj.computation_interval);
  return ret;
}

void dec_json(Json::Value const& json, SortitionConfig& obj) {
  dec_json(json, dynamic_cast<SortitionParams&>(obj));
  obj.changes_count_for_average = dev::jsToInt(json["changes_count_for_average"].asString());
  obj.max_interval_correction = dev::jsToInt(json["max_interval_correction"].asString());

  auto first = dev::jsToInt(json["dag_efficiency_targets"][0].asString());
  auto second = dev::jsToInt(json["dag_efficiency_targets"][1].asString());
  obj.dag_efficiency_targets = {first, second};

  obj.computation_interval = dev::jsToInt(json["computation_interval"].asString());
}

}  // namespace taraxa