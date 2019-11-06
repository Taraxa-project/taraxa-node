#ifndef TARAXA_NODE_ACCOUNT_STATE_STATE_HPP_
#define TARAXA_NODE_ACCOUNT_STATE_STATE_HPP_

#include "thread_safe_state.hpp"

namespace taraxa::account_state::state_registry {
class StateRegistry;
};

namespace taraxa::account_state::state {
namespace eth = dev::eth;
using dev::OverlayDB;
using dev::u256;
using state_registry::StateRegistry;
using std::unique_lock;
using thread_safe_state::ThreadSafeState;

struct State : ThreadSafeState {
  friend class StateRegistry;

  StateRegistry *const host_;
  StateSnapshot snapshot_;

  State(decltype(host_) &host,
        decltype(snapshot_) const &snapshot,  //
        u256 const &account_start_nonce,      //
        OverlayDB const &db)
      : host_(host),
        snapshot_(snapshot),
        ThreadSafeState(account_start_nonce, db, eth::BaseState::Empty) {
    setRoot(snapshot.state_root);
  }

 public:
  State &operator=(State const &s) {
    if (&s != this) {
      assert(host_ == s.host_);
      unique_lock l(m_);
      eth::State::operator=(s);
      snapshot_ = s.snapshot_;
    }
    return *this;
  }

  StateSnapshot getSnapshot() {
    unique_lock l(m_);
    return snapshot_;
  }

 private:
  void setSnapshot(StateSnapshot const &snapshot) {
    unique_lock l(m_);
    snapshot_ = snapshot;
  }

  using eth::State::setRoot;
  using ThreadSafeState::commitAndPush;
};

}  // namespace taraxa::account_state::state

#endif  // TARAXA_NODE_ACCOUNT_STATE_STATE_HPP_
