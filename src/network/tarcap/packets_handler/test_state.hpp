#pragma once

#include "dag/dag_block.hpp"
#include "transaction_manager/transaction.hpp"

namespace taraxa::network::tarcap {

// THIS NEEDS TO BE REMOVED !!!!!!!
class TestState {
 public:
  std::unordered_map<blk_hash_t, DagBlock> test_blocks_;
  std::unordered_map<trx_hash_t, Transaction> test_transactions_;
};

}  // namespace taraxa::network::tarcap