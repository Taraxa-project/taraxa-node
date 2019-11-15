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

class TrxEngine {
  string go_address_;

 public:
  class Exception : public runtime_error {
    using runtime_error::runtime_error;
  };

  class CreationException : public Exception {
    using Exception::Exception;
  };
  explicit TrxEngine(shared_ptr<DatabaseFace> db);

  ~TrxEngine();

  class TransitionStateException : public Exception {
    using Exception::Exception;
  };
  StateTransitionResult transitionState(StateTransitionRequest const& req);

  class CommitToDiskException : public Exception {
    using Exception::Exception;
  };
  void commitToDisk(root_t const& state_root);
};

}  // namespace taraxa::trx_engine::trx_engine

#endif
