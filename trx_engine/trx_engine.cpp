#include "trx_engine.hpp"

namespace taraxa::trx_engine::trx_engine {

TrxEngine::TrxEngine(decltype(accs_state_db_) const& accs_state_db) {}

TrxEngine::~TrxEngine() {}

StateTransitionResult TrxEngine::transitionStateAndCommit(
    StateTransitionRequest const& req) {
  return {};
}

};  // namespace taraxa::trx_engine::trx_engine