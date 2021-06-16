#pragma once

#include <shared_mutex>

#include "dag/dag_block.hpp"
#include "transaction_manager/transaction.hpp"

namespace taraxa::network::tarcap {

// TODO: THIS NEEDS TO BE REMOVED !!!!!!!
class TestState {
 public:
  bool hasTransaction(const trx_hash_t& tx_hash) const;
  void insertTransaction(const Transaction& tx);
  const Transaction& getTransaction(const trx_hash_t& tx_hash) const;
  size_t getTransactionsSize() const;

  bool hasBlock(const blk_hash_t& block_hash) const;
  void insertBlock(const DagBlock& tx);
  const DagBlock& getBlock(const blk_hash_t& block_hash) const;
  size_t getBlocksSize() const;

  std::unordered_map<trx_hash_t, Transaction> getTransactions();
  std::unordered_map<blk_hash_t, DagBlock> getBlocks();

 private:
  std::unordered_map<trx_hash_t, Transaction> test_transactions_;
  mutable std::shared_mutex transactions_mutex_;

  std::unordered_map<blk_hash_t, DagBlock> test_blocks_;
  mutable std::shared_mutex blocks_mutex_;
};

}  // namespace taraxa::network::tarcap