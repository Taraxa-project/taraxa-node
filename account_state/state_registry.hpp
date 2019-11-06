#ifndef TARAXA_NODE_ACCOUNT_STATE_STATE_REGISTRY_HPP
#define TARAXA_NODE_ACCOUNT_STATE_STATE_REGISTRY_HPP

#include <atomic>
#include <memory>
#include <mutex>
#include <optional>
#include <utility>
#include <vector>
#include "genesis_state.hpp"
#include "state.hpp"
#include "state_snapshot.hpp"
#include "thread_safe_state.hpp"
#include "types.hpp"

namespace taraxa::account_state::state_registry {
namespace eth = dev::eth;
using dev::OverlayDB;
using dev::db::DatabaseFace;
using state::State;
using state_snapshot::StateSnapshot;
using std::atomic;
using std::mutex;
using std::optional;
using std::pair;
using std::unique_ptr;
using std::vector;

struct StateRegistry {
  using batch_t = vector<pair<blk_hash_t, root_t>>;

 private:
  uint256_t const account_start_nonce_;
  DatabaseFace *account_db_raw_;
  OverlayDB account_db_;
  unique_ptr<DatabaseFace> snapshot_db_;
  atomic<StateSnapshot> current_snapshot_;
  mutex m_;

 public:
  StateRegistry(GenesisState const &genesis_state,
                unique_ptr<DatabaseFace> account_db,  //
                unique_ptr<DatabaseFace> snapshot_db);
  auto getAccountDbRaw() { return account_db_raw_; }
  void append(batch_t const &blk_to_root) { append(blk_to_root, false); }
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
  optional<StateSnapshot> getSnapshotFromDB(dag_blk_num_t const &);
  void append(batch_t const &blk_to_root, bool init);
};

}  // namespace taraxa::account_state::state_registry

#endif
