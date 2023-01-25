#include "Debug.h"

using namespace std;
using namespace dev;
using namespace ::taraxa::final_chain;
using namespace jsonrpc;
using namespace taraxa;

namespace taraxa::net {

Json::Value Debug::debug_traceTransaction(const std::string& /*param1*/, const Json::Value& /*param2*/) {
  Json::Value res;
//   try {
//     if (auto node = full_node_.lock()) {
//       uint64_t period = param1["period"].asUInt64();
//       auto params_change = node->getDB()->getParamsChangeForPeriod(period);
//       res["interval_efficiency"] = params_change->interval_efficiency;
//       res["period"] = params_change->period;
//       res["threshold_upper"] = params_change->vrf_params.threshold_upper;
//       res["kThresholdUpperMinValue"] = params_change->vrf_params.kThresholdUpperMinValue;
//     }
//   } catch (std::exception &e) {
//     res["status"] = e.what();
//   }
  return res;
}

Json::Value Debug::debug_traceCall(const Json::Value& /*param1*/, const std::string& /*param2*/, const Json::Value& /*param3*/) {
  Json::Value res;
//   try {
//     if (auto node = full_node_.lock()) {
//       uint64_t period = param1["period"].asUInt64();
//       auto params_change = node->getDB()->getParamsChangeForPeriod(period);
//       res["interval_efficiency"] = params_change->interval_efficiency;
//       res["period"] = params_change->period;
//       res["threshold_upper"] = params_change->vrf_params.threshold_upper;
//       res["kThresholdUpperMinValue"] = params_change->vrf_params.kThresholdUpperMinValue;
//     }
//   } catch (std::exception &e) {
//     res["status"] = e.what();
//   }
  return res;
}

} // namespace taraxa::net