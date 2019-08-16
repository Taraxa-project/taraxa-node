#include "state_registry.hpp"
#include <iostream>
#include <string>
#include "util_eth.hpp"

namespace taraxa::state_registry {
using State = StateRegistry::State;
using util::eth::calculateGenesisState;
using util::eth::toSlice;

void StateRegistry::init(GenesisState const &genesis_state) {
  eth::AccountMap genesis_accs_eth;
  for (auto &acc : genesis_state.accounts) {
    genesis_accs_eth[acc.first] = {account_start_nonce_, acc.second.balance};
  }
  auto const &genesis_hash = genesis_state.block.getHash();
  auto current_block_num = snapshot_db_->lookup(CURRENT_BLOCK_NUMBER_KEY);
  if (current_block_num.empty()) {
    auto genesis_root = calculateGenesisState(account_db_, genesis_accs_eth);
    account_db_.commit();
    append(genesis_root, {genesis_hash}, true);
    return;
  }
  auto genesis_snapshot = *getSnapshot(0);
  StateCacheDB tmp_mem_db;
  auto genesis_root = calculateGenesisState(tmp_mem_db, genesis_accs_eth);
  assert(genesis_snapshot.state_root == genesis_root);
  assert(genesis_snapshot.block_hash == genesis_hash);
  current_snapshot_ = *getSnapshot(stoull(current_block_num));
}

void StateRegistry::commitAndPush(
    State &state,  //
    vector<blk_hash_t> const &blks,
    eth::State::CommitBehaviour const &commit_behaviour) {
  assert(state.host_ == this);
  unique_lock l(m_);
  assert(state.getSnapshot() == getCurrentSnapshot());
  append(state.commitAndPush(commit_behaviour), blks);
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
  auto snapshot_str = snapshot_db_->lookup(blkNumKey(blk_num));
  if (snapshot_str.empty()) {
    return nullopt;
  }
  RLP snapshot_rlp(snapshot_str);
  return {{
      snapshot_rlp[0].toInt<decltype(StateSnapshot::block_number)>(),
      snapshot_rlp[1].toHash<decltype(StateSnapshot::block_hash)>(),
      snapshot_rlp[2].toHash<decltype(StateSnapshot::state_root)>(),
  }};
}

optional<StateSnapshot> StateRegistry::getSnapshot(blk_hash_t const &blk_hash) {
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

void StateRegistry::append(root_t const &state_root,
                           vector<blk_hash_t> const &blk_hashes, bool init) {
  assert(!blk_hashes.empty());
  auto batch = snapshot_db_->createWriteBatch();
  auto blk_num = init ? 0 : getCurrentSnapshot().block_number + 1;
  for (auto &blk_hash : blk_hashes) {
    auto blk_hash_key = blkHashKey(blk_hash);
    if (blk_num > 1) {
      // TODO remove after the fix for genesis block overlap bug
      assert(snapshot_db_->lookup(blk_hash_key).empty());
    }
    RLPStream snapshot_rlp(3);
    snapshot_rlp << blk_num << blk_hash << state_root;
    batch->insert(blkNumKey(blk_num), toSlice(snapshot_rlp.out()));
    batch->insert(blk_hash_key, to_string(blk_num));
    ++blk_num;
  }
  --blk_num;
  batch->insert(CURRENT_BLOCK_NUMBER_KEY, to_string(blk_num));
  snapshot_db_->commit(move(batch));
  current_snapshot_ = {blk_num, blk_hashes.back(), state_root};
}

}  // namespace taraxa::state_registry