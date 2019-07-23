#ifndef TARAXA_NODE_STATEREGISTRY_HPP
#define TARAXA_NODE_STATEREGISTRY_HPP

#include <libdevcore/CommonData.h>
#include <memory>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <tuple>
#include <utility>
#include <vector>
#include "GenesisState.hpp"
#include "SimpleDBFace.h"
#include "types.hpp"

namespace taraxa::__StateRegistry__ {
using namespace std;
using namespace dev;

class StateRegistry {
 public:
  class State : public eth::State {
    friend class StateRegistry;

   private:
    State(u256 const &account_start_nonce, OverlayDB const &db,
          root_t const &state_root)
        : eth::State(account_start_nonce, db) {
      setRoot(state_root);
    }

    OverlayDB &db() { return eth::State::db(); }
    void populateFrom(eth::AccountMap const &account_map) {
      eth::State::populateFrom(account_map);
    }
    void commit(CommitBehaviour commit_behaviour) {
      eth::State::commit(commit_behaviour);
    }
    void setRoot(root_t const &root) { eth::State::setRoot(root); }
  };

  struct Snapshot {
    dag_blk_num_t dag_block_number;
    blk_hash_t dag_block_hash;
    root_t state_root;
  };

  static inline string const CURRENT_BLOCK_NUMBER_KEY = "blk_num_current";
  static inline string const BLOCK_HASH_KEY_PREFIX = "blk_hash_";
  static inline string const BLOCK_NUMBER_KEY_PREFIX = "blk_num_";

 private:
  uint256_hash_t const account_start_nonce_;
  OverlayDB account_db_;
  unique_ptr<db::DatabaseFace> snapshot_db_;
  Snapshot current_snapshot_;
  shared_mutex m_;

  void init(GenesisState const &);

 public:
  StateRegistry(GenesisState const &genesis_state,
                decltype(account_db_) account_db,  //
                decltype(snapshot_db_) snapshot_db)
      : account_start_nonce_(genesis_state.account_start_nonce),
        account_db_(move(account_db)),
        snapshot_db_(move(snapshot_db)) {
    init(genesis_state);
  }

  static string blkNumKey(dag_blk_num_t const &x) {
    return BLOCK_NUMBER_KEY_PREFIX + to_string(x);
  }

  static string blkHashKey(blk_hash_t const &x) {
    return BLOCK_HASH_KEY_PREFIX + x.hex();
  }

  Snapshot commit(State &, vector<blk_hash_t> const &,
                  eth::State::CommitBehaviour const & =
                      eth::State::CommitBehaviour::KeepEmptyAccounts);
  optional<Snapshot> getSnapshot(dag_blk_num_t const &);
  Snapshot getCurrentSnapshot();
  State getCurrentState();
  optional<State> getState(dag_blk_num_t const &);
  optional<dag_blk_num_t> getNumber(blk_hash_t const &);

 private:
  void append(root_t const &, vector<blk_hash_t> const &, bool init = false);
};

}  // namespace taraxa::__StateRegistry__

namespace taraxa {
using __StateRegistry__::StateRegistry;
}

#endif  // TARAXA_NODE_STATEREGISTRY_HPP
