#pragma once

#include "dag/dag_block.hpp"
#include "transaction/system_transaction.hpp"

namespace taraxa::network::tarcap {

struct DagBlockPacket {
  std::vector<std::shared_ptr<Transaction>> transactions;
  std::shared_ptr<DagBlock> dag_block;

  RLP_FIELDS_DEFINE_INPLACE(transactions, dag_block)
};

}  // namespace taraxa::network::tarcap
