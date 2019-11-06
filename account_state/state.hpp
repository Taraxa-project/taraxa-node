#ifndef TARAXA_NODE_ACCOUNT_STATE_STATE_HPP_
#define TARAXA_NODE_ACCOUNT_STATE_STATE_HPP_

#include "state_snapshot.hpp"
#include "thread_safe_state.hpp"

namespace taraxa::account_state::state_registry {
class StateRegistry;
};

namespace taraxa::account_state::state {
namespace eth = dev::eth;
using dev::OverlayDB;
using dev::u256;
using state_registry::StateRegistry;
using state_snapshot::StateSnapshot;
using std::unique_lock;
using thread_safe_state::ThreadSafeState;

class State : public ThreadSafeState {
  friend class StateRegistry;

  StateRegistry *const host_;
  StateSnapshot snapshot_;

  State(decltype(host_) &host,
        decltype(snapshot_) const &snapshot,  //
        u256 const &account_start_nonce,      //
        OverlayDB const &db);

 public:
  State &operator=(State const &s);
  StateSnapshot getSnapshot();

 private:
  using eth::State::setRoot;
  using ThreadSafeState::commitAndPush;

  void setSnapshot(StateSnapshot const &snapshot);
};

}  // namespace taraxa::account_state::state

#endif  // TARAXA_NODE_ACCOUNT_STATE_STATE_HPP_
