#include "trx_engine.hpp"

#include <json/value.h>
#include <libdevcore/CommonJS.h>

#include <stdexcept>
#include <utility>

#include "db/cgo/db.hpp"
#include "util.hpp"
#include "util/once_out_of_scope.hpp"
#include "util_json.hpp"

namespace taraxa::trx_engine::trx_engine {
using namespace std;
using namespace dev;
using db::cgo::AlethDatabaseAdapter;
using util::once_out_of_scope::OnceOutOfScope;

// TODO less copy-paste
TrxEngine::TrxEngine(decltype(accs_state_db_) const& accs_state_db)
    : accs_state_db_(accs_state_db) {
  Json::Value args(Json::arrayValue);
  auto db_adapter = new AlethDatabaseAdapter(accs_state_db_.get());
  args[0] = reinterpret_cast<Json::UInt64>(&db_adapter->c_self);
  char* cgo_retval = taraxa_cgo_env_Call(nullptr,
                                         cgo_str("NewTaraxaTrxEngine"),  //
                                         cgo_str(util_json::toString(args)));
  OnceOutOfScope _([&] { taraxa_cgo_free(cgo_retval); });
  const auto& result_json = util_json::fromString(string_view(cgo_retval));
  go_address_ = result_json[0].asString();
  if (auto const& err = result_json[1]; !err.isNull()) {
    throw Exception(err.asString());
  }
}

TrxEngine::~TrxEngine() { taraxa_cgo_env_Free(cgo_str(go_address_)); }

StateTransitionResult TrxEngine::transitionStateAndCommit(
    StateTransitionRequest const& req) {
  Json::Value args(Json::arrayValue);
  args[0] = req.toJson();
  char* cgo_retval =
      taraxa_cgo_env_Call(cgo_str(go_address_),
                          cgo_str("TransitionStateAndCommit"),  //
                          cgo_str(util_json::toString(args)));
  OnceOutOfScope _([&] { taraxa_cgo_free(cgo_retval); });
  const auto& result_json = util_json::fromString(string_view(cgo_retval));
  auto const& result = result_json[0];
  if (auto const& err = result_json[1]; !err.isNull()) {
    throw Exception(err.asString());
  }
  return StateTransitionResult::fromJson(result);
}

};  // namespace taraxa::trx_engine::trx_engine