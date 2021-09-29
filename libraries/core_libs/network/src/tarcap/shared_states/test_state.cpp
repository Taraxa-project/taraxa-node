#include "network/tarcap/shared_states/test_state.hpp"

namespace taraxa::network::tarcap {

bool TestState::hasTransaction(const trx_hash_t& tx_hash) const {
  std::shared_lock lock(transactions_mutex_);
  return test_transactions_.count(tx_hash);
}

void TestState::insertTransaction(const Transaction& tx) {
  std::unique_lock lock(transactions_mutex_);
  test_transactions_.emplace(tx.getHash(), tx);
}

const Transaction& TestState::getTransaction(const trx_hash_t& tx_hash) const {
  std::shared_lock lock(transactions_mutex_);
  assert(test_transactions_.count(tx_hash));

  return test_transactions_.at(tx_hash);
}

size_t TestState::getTransactionsSize() const {
  std::shared_lock lock(transactions_mutex_);
  return test_transactions_.size();
}

bool TestState::hasBlock(const blk_hash_t& block_hash) const {
  std::shared_lock lock(blocks_mutex_);
  return test_blocks_.count(block_hash);
}

void TestState::insertBlock(const DagBlock& block) {
  std::unique_lock lock(blocks_mutex_);
  test_blocks_.emplace(block.getHash(), block);
}

const DagBlock& TestState::getBlock(const blk_hash_t& block_hash) const {
  std::shared_lock lock(blocks_mutex_);
  assert(test_blocks_.count(block_hash));

  return test_blocks_.at(block_hash);
}

size_t TestState::getBlocksSize() const {
  std::shared_lock lock(blocks_mutex_);
  return test_blocks_.size();
}

std::unordered_map<trx_hash_t, Transaction> TestState::getTransactions() {
  std::shared_lock lock(transactions_mutex_);
  return test_transactions_;
}

std::unordered_map<blk_hash_t, DagBlock> TestState::getBlocks() {
  std::shared_lock lock(blocks_mutex_);
  return test_blocks_;
}

}  // namespace taraxa::network::tarcap