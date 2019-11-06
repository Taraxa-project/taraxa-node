#include "state.hpp"

namespace taraxa::account_state::state {

State::State(decltype(host_)& host, decltype(snapshot_) const& snapshot,
             dev::u256 const& account_start_nonce, dev::OverlayDB const& db)
    : host_(host),
      snapshot_(snapshot),
      ThreadSafeState(account_start_nonce, db, eth::BaseState::Empty) {
  setRoot(snapshot.state_root);
}

State& State::operator=(State const& s) {
  if (&s != this) {
    assert(host_ == s.host_);
    unique_lock l(m_);
    eth::State::operator=(s);
    snapshot_ = s.snapshot_;
  }
  return *this;
}

StateSnapshot State::getSnapshot() {
  unique_lock l(m_);
  return snapshot_;
}

void State::setSnapshot(StateSnapshot const& snapshot) {
  unique_lock l(m_);
  snapshot_ = snapshot;
}

};