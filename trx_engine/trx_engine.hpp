#ifndef TARAXA_NODE_TRX_ENGINE_TRX_ENGINE_HPP_
#define TARAXA_NODE_TRX_ENGINE_TRX_ENGINE_HPP_

extern "C" {
#include "trx_engine.h"
}
#include <json/value.h>
#include <libdevcore/db.h>
#include <string>
#include "types.hpp"

namespace taraxa::trx_engine {

class TrxEngine {
  std::string go_address_;

 public:
  explicit TrxEngine(std::shared_ptr<dev::db::DatabaseFace> db);
  ~TrxEngine();

  StateTransitionResult transitionState(StateTransitionRequest const& req);
  void commitToDisk(taraxa::root_t const& state_root);
};

}  // namespace taraxa::trx_engine

#endif
