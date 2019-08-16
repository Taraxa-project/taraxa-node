#ifndef TARAXA_NODE_STATEREGISTRY_HPP
#define TARAXA_NODE_STATEREGISTRY_HPP

#include <atomic>
#include <memory>
#include <mutex>
#include <optional>
#include <utility>
#include <vector>
#include "SimpleDBFace.h"
#include "genesis_state.hpp"
#include "thread_safe_state.hpp"
#include "types.hpp"

namespace taraxa::state_registry {
using namespace std;
using namespace dev;

class StateRegistry {
 public:
  class State : public ThreadSafeState {
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

  static inline string const CURRENT_BLOCK_NUMBER_KEY = "blk_num_current";
  static inline string const BLOCK_HASH_KEY_PREFIX = "blk_hash_";
  static inline string const BLOCK_NUMBER_KEY_PREFIX = "blk_num_";

 private:
  uint256_t const account_start_nonce_;
  OverlayDB account_db_;
  unique_ptr<db::DatabaseFace> snapshot_db_;
  atomic<StateSnapshot> current_snapshot_;
  mutex m_;

  void init(GenesisState const &);

 public:
  StateRegistry(GenesisState const &genesis_state,
                decltype(account_db_) account_db,  //
                decltype(snapshot_db_) snapshot_db)
      : account_start_nonce_(genesis_state.account_start_nonce),
        account_db_(move(account_db)),
        snapshot_db_(move(snapshot_db)),
        current_snapshot_(StateSnapshot()) {
    init(genesis_state);
  }

  static string blkNumKey(dag_blk_num_t const &x) {
    return BLOCK_NUMBER_KEY_PREFIX + to_string(x);
  }

  static string blkHashKey(blk_hash_t const &x) {
    return BLOCK_HASH_KEY_PREFIX + x.hex();
  }

  void commitAndPush(State &,
                     vector<blk_hash_t> const &,  //
                     eth::State::CommitBehaviour const & =
                         eth::State::CommitBehaviour::KeepEmptyAccounts);
  State &rebase(State &);
  optional<StateSnapshot> getSnapshot(dag_blk_num_t const &);
  optional<StateSnapshot> getSnapshot(blk_hash_t const &);
  StateSnapshot getCurrentSnapshot();
  State getCurrentState();
  optional<State> getState(dag_blk_num_t const &);
  optional<dag_blk_num_t> getNumber(blk_hash_t const &);

 private:
  void append(root_t const &, vector<blk_hash_t> const &, bool init = false);
};

}  // namespace taraxa::state_registry

namespace taraxa {
using state_registry::StateRegistry;
}

#endif  // TARAXA_NODE_STATEREGISTRY_HPP
