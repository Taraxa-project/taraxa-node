#include "trx_engine.hpp"
#include <json/value.h>
#include <libdevcore/CommonJS.h>
#include <stdexcept>
#include <utility>
#include "db/cgo/db.hpp"
#include "util.hpp"
#include "util_json.hpp"

namespace taraxa::trx_engine {
using namespace std;
using namespace dev;

// TODO less copy-paste
// TODO error handling

TrxEngine::TrxEngine(std::shared_ptr<dev::db::DatabaseFace> db) {
  Json::Value args(Json::arrayValue);
  auto db_adapter = new db::cgo::AlethDatabaseAdapter(db);
  args[0] = reinterpret_cast<Json::UInt64>(db_adapter->c_self());
  char* resultJsonStr = taraxa_cgo_env_Call(nullptr,
                                            cgo_str("NewTaraxaTrxEngine"),  //
                                            cgo_str(util_json::toString(args)));
  const auto& cgo_retval = util_json::fromString(string_view(resultJsonStr));
  taraxa_cgo_free(resultJsonStr);
  go_address_ = cgo_retval[0].asString();
  auto err = cgo_retval[1].asString();
  if (!err.empty()) {
    throw runtime_error(err);
  }
}

TrxEngine::~TrxEngine() { taraxa_cgo_env_Free(cgo_str(go_address_)); }

StateTransitionResult TrxEngine::transitionState(
    StateTransitionRequest const& req) {
  Json::Value args(Json::arrayValue);
  args[0] = req.toJson();
  char* cgo_retval = taraxa_cgo_env_Call(cgo_str(go_address_),
                                         cgo_str("TransitionState"),  //
                                         cgo_str(util_json::toString(args)));
  const auto& result_json = util_json::fromString(string_view(cgo_retval));
  taraxa_cgo_free(cgo_retval);
  auto const& result = result_json[0];
  auto const& err = result_json[1];
  if (!err.isNull()) {
    //    throw runtime_error(err.asString());
  }
  StateTransitionResult ret;
  ret.fromJson(result);
  return ret;
}

void TrxEngine::commitToDisk(taraxa::root_t const& state_root) {
  Json::Value args(Json::arrayValue);
  args[0] = dev::toJS(state_root);
  char* cgo_retval = taraxa_cgo_env_Call(cgo_str(go_address_),
                                         cgo_str("CommitToDisk"),  //
                                         cgo_str(util_json::toString(args)));
  auto const& result_json = util_json::fromString(string_view(cgo_retval));
  taraxa_cgo_free(cgo_retval);
  auto const& err = result_json[0].asString();
  if (!err.empty()) {
    throw runtime_error(err);
  }
}

};