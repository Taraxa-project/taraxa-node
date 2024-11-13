#pragma once

#include "dag/dag_block.hpp"
#include "transaction/system_transaction.hpp"

namespace taraxa::network::tarcap {

struct DagSyncPacket {
  PbftPeriod request_period;
  PbftPeriod response_period;
  std::vector<std::shared_ptr<Transaction>> transactions;
  std::vector<std::shared_ptr<DagBlock>> dag_blocks;

  RLP_FIELDS_DEFINE_INPLACE(request_period, response_period, transactions, dag_blocks)
};

}  // namespace taraxa::network::tarcap
