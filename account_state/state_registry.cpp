#include "state_registry.hpp"
#include <iostream>
#include <string>
#include <unordered_set>
#include "state.hpp"
#include "util/eth.hpp"

namespace taraxa::account_state::state_registry {
using dev::RLP;
using dev::RLPStream;
using dev::StateCacheDB;
using std::nullopt;
using std::to_string;
using std::unique_lock;
using std::unordered_set;
using util::eth::calculateGenesisState;
using util::eth::toSlice;

string const CURRENT_BLOCK_NUMBER_KEY = "blk_num_current";

string blkNumKey(dag_blk_num_t const &x) { return "blk_num_" + to_string(x); }

string blkHashKey(blk_hash_t const &x) { return "blk_hash_" + x.hex(); }

StateRegistry::StateRegistry(GenesisState const &genesis_state,
                             unique_ptr<DatabaseFace> account_db,
                             unique_ptr<DatabaseFace> snapshot_db)
    : account_start_nonce_(genesis_state.account_start_nonce),
      account_db_raw_(account_db.get()),
      account_db_(OverlayDB(move(account_db))),
      snapshot_db_(move(snapshot_db)),
      current_snapshot_(StateSnapshot()) {
  eth::AccountMap genesis_accs_eth;
  for (auto &acc : genesis_state.accounts) {
    genesis_accs_eth[acc.first] = {account_start_nonce_, acc.second.balance};
  }
  auto const &genesis_hash = genesis_state.block.getHash();
  auto current_block_num = snapshot_db_->lookup(CURRENT_BLOCK_NUMBER_KEY);
  if (current_block_num.empty()) {
    auto genesis_root = calculateGenesisState(account_db_, genesis_accs_eth);
    account_db_.commit();
    append({{genesis_hash, genesis_root}}, true);
    return;
  }
  auto genesis_snapshot = *getSnapshotFromDB(0);
  StateCacheDB tmp_mem_db;
  auto genesis_root = calculateGenesisState(tmp_mem_db, genesis_accs_eth);
  assert(genesis_snapshot.state_root == genesis_root);
  assert(genesis_snapshot.block_hash == genesis_hash);
  if (auto blk_num = stoull(current_block_num); blk_num != 0) {
    current_snapshot_ = *getSnapshot(blk_num);
  } else {
    current_snapshot_ = genesis_snapshot;
  }
}

void StateRegistry::commitAndPush(
    State &state,  //
    vector<blk_hash_t> const &blks,
    eth::State::CommitBehaviour const &commit_behaviour) {
  assert(state.host_ == this);
  assert(state.getSnapshot() == getCurrentSnapshot());
  auto const &root = state.commitAndPush(commit_behaviour);
  batch_t blk_to_root;
  for (auto const &blk : blks) {
    blk_to_root.push_back({blk, root});
  }
  append(blk_to_root);
  state.setSnapshot(current_snapshot_);
}

State &StateRegistry::rebase(State &state) {
  assert(state.host_ == this);
  if (state.getSnapshot() != getCurrentSnapshot()) {
    state = getCurrentState();
  }
  return state;
}

optional<StateSnapshot> StateRegistry::getSnapshot(
    dag_blk_num_t const &blk_num) {
  if (auto s = current_snapshot_.load(); s.block_number == blk_num) {
    return s;
  }
  return getSnapshotFromDB(blk_num);
}

optional<StateSnapshot> StateRegistry::getSnapshot(blk_hash_t const &blk_hash) {
  if (auto s = current_snapshot_.load(); s.block_hash == blk_hash) {
    return s;
  }
  auto const &block_number_opt = getNumber(blk_hash);
  return block_number_opt ? getSnapshot(*block_number_opt) : nullopt;
}

StateSnapshot StateRegistry::getCurrentSnapshot() { return current_snapshot_; }

State StateRegistry::getCurrentState() {
  return {this, getCurrentSnapshot(), account_start_nonce_, account_db_};
}

optional<State> StateRegistry::getState(dag_blk_num_t const &blk_num) {
  auto snapshot = getSnapshot(blk_num);
  if (!snapshot) {
    return nullopt;
  }
  if (account_db_.lookup(snapshot->state_root).empty()) {
    return nullopt;
  }
  return {{this, *snapshot, account_start_nonce_, account_db_}};
}

optional<dag_blk_num_t> StateRegistry::getNumber(blk_hash_t const &blk_hash) {
  auto blk_num_str = snapshot_db_->lookup(blkHashKey(blk_hash));
  return blk_num_str.empty() ? nullopt : optional(stoull(blk_num_str));
}

optional<StateSnapshot> StateRegistry::getSnapshotFromDB(
    dag_blk_num_t const &blk_num) {
  auto snapshot_str = snapshot_db_->lookup(blkNumKey(blk_num));
  if (snapshot_str.empty()) {
    return nullopt;
  }
  return StateSnapshot::make(RLP(snapshot_str));
}

void StateRegistry::append(batch_t const &blk_to_root, bool init) {
  assert(!blk_to_root.empty());
  unique_lock l(m_);
  auto batch = snapshot_db_->createWriteBatch();
  unordered_set<blk_hash_t> blk_processed;
  auto blk_num = init ? 0 : getCurrentSnapshot().block_number + 1;
  for (auto &[blk_hash, state_root] : blk_to_root) {
    assert(blk_processed.insert(blk_hash).second);
    auto blk_hash_key = blkHashKey(blk_hash);
    // The assert checks double commit a block
    assert(snapshot_db_->lookup(blk_hash_key).empty());
    StateSnapshot s{blk_num, blk_hash, state_root};
    batch->insert(blkNumKey(blk_num), toSlice(s.rlp().out()));
    batch->insert(blk_hash_key, to_string(blk_num));
    ++blk_num;
  }
  --blk_num;
  batch->insert(CURRENT_BLOCK_NUMBER_KEY, to_string(blk_num));
  snapshot_db_->commit(move(batch));
  auto &[last_blk, last_root] = blk_to_root.back();
  current_snapshot_ = {blk_num, last_blk, last_root};
}

}  // namespace taraxa::account_state::state_registry