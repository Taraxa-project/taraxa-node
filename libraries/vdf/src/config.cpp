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
  ret["threshold_range"] = obj.threshold_range;
  return ret;
}
void dec_json(Json::Value const& json, VrfParams& obj) {
  obj.threshold_upper = dev::jsToInt(json["threshold_upper"].asString());
  obj.threshold_range = json["threshold_range"].asInt();
}

Json::Value enc_json(VdfParams const& obj) {
  Json::Value ret(Json::objectValue);
  ret["difficulty_min"] = obj.difficulty_min;
  ret["difficulty_max"] = obj.difficulty_max;
  ret["difficulty_stale"] = obj.difficulty_stale;
  ret["lambda_bound"] = dev::toJS(obj.lambda_bound);
  return ret;
}

void dec_json(Json::Value const& json, VdfParams& obj) {
  obj.difficulty_min = json["difficulty_min"].asInt();
  obj.difficulty_max = json["difficulty_max"].asInt();
  obj.difficulty_stale = json["difficulty_stale"].asInt();
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
  ret["changes_count_for_average"] = obj.changes_count_for_average;

  Json::Value targets(Json::arrayValue);
  targets.append(obj.dag_efficiency_targets.first);
  targets.append(obj.dag_efficiency_targets.second);

  ret["dag_efficiency_targets"] = targets;
  ret["changing_interval"] = obj.changing_interval;
  ret["computation_interval"] = obj.computation_interval;
  return ret;
}

void dec_json(Json::Value const& json, SortitionConfig& obj) {
  dec_json(json, dynamic_cast<SortitionParams&>(obj));
  obj.changes_count_for_average = json["changes_count_for_average"].asInt();

  auto first = json["dag_efficiency_targets"][0].asInt();
  auto second = json["dag_efficiency_targets"][1].asInt();
  obj.dag_efficiency_targets = {first, second};

  obj.changing_interval = json["changing_interval"].asInt();
  obj.computation_interval = json["computation_interval"].asInt();
}

}  // namespace taraxa