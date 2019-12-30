#ifndef TARAXA_NODE_TRX_ENGINE_TRX_ENGINE_HPP_
#define TARAXA_NODE_TRX_ENGINE_TRX_ENGINE_HPP_

extern "C" {
#include "taraxa_evm_cgo.h"
}
#include <json/value.h>
#include <libdevcore/db.h>

#include <stdexcept>
#include <string>

#include "types.hpp"

namespace taraxa::trx_engine::trx_engine {
using dev::db::DatabaseFace;
using std::runtime_error;
using std::shared_ptr;
using std::string;
using types::StateTransitionRequest;
using types::StateTransitionResult;

struct TrxEngine {
  struct Exception : runtime_error {
    using runtime_error::runtime_error;
  };

 private:
  shared_ptr<DatabaseFace> accs_state_db_;
  string go_address_;

 public:
  explicit TrxEngine(decltype(accs_state_db_) const& accs_state_db);
  ~TrxEngine();

  StateTransitionResult transitionStateAndCommit(
      StateTransitionRequest const& req);
};

}  // namespace taraxa::trx_engine::trx_engine

#endif
