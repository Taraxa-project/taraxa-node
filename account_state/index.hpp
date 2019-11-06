#ifndef TARAXA_NODE_ACCOUNT_STATE_INDEX_HPP_
#define TARAXA_NODE_ACCOUNT_STATE_INDEX_HPP_

#include "state.hpp"
#include "state_registry.hpp"
#include "thread_safe_state.hpp"

namespace taraxa::account_state {
using state::State;
using state_registry::StateRegistry;
using thread_safe_state::ThreadSafeState;
};  // namespace taraxa::account_state

#endif  // TARAXA_NODE_ACCOUNT_STATE_INDEX_HPP_
