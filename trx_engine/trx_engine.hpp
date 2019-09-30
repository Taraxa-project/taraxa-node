#ifndef TARAXA_NODE_TRX_ENGINE_TRX_ENGINE_HPP_
#define TARAXA_NODE_TRX_ENGINE_TRX_ENGINE_HPP_

extern "C" {
#include "trx_engine.h"
}
#include <json/value.h>
#include <libdevcore/db.h>
#include <stdexcept>
#include <string>
#include "types.hpp"

namespace taraxa::trx_engine {

class TrxEngine {
  std::string go_address_;

 public:
  class Exception : public std::runtime_error {
    using std::runtime_error::runtime_error;
  };

  class CreationException : public Exception {
    using Exception::Exception;
  };
  explicit TrxEngine(std::shared_ptr<dev::db::DatabaseFace> db);

  ~TrxEngine();

  class TransitionStateException : public Exception {
    using Exception::Exception;
  };
  StateTransitionResult transitionState(StateTransitionRequest const& req);

  class CommitToDiskException : public Exception {
    using Exception::Exception;
  };
  void commitToDisk(taraxa::root_t const& state_root);
};

}  // namespace taraxa::trx_engine

#endif
