#pragma once

#include <atomic>
#include <condition_variable>
#include <iostream>
#include <memory>
#include <set>
#include <thread>

#include "common/types.hpp"
#include "common/util.hpp"
#include "config/config.hpp"
#include "consensus/pbft_chain.hpp"
#include "dag/dag_block.hpp"
#include "storage/db_storage.hpp"

namespace taraxa {

class FullNode;

using TransactionOverlapTable = std::vector<std::pair<blk_hash_t, std::vector<bool>>>;
enum class TransactionExecStatus { non_ordered, ordered, executed };
using TransactionExecStatusTable = StatusTable<trx_hash_t, TransactionExecStatus>;
using TrxOverlapInBlock = std::pair<blk_hash_t, std::vector<bool>>;

class TransactionOrderManager {
 public:
  TransactionOrderManager(addr_t const& node_addr, std::shared_ptr<DbStorage> db) : db_(std::move(db)) {
    LOG_OBJECTS_CREATE("TRXORD");
  }
  void clear() { status_.clear(); }

  std::vector<bool> computeOrderInBlock(DagBlock const& blk, TransactionExecStatusTable& status_for_proposing_blocks);

 private:
  TransactionExecStatusTable status_;
  std::shared_ptr<DbStorage> db_;
  LOG_OBJECTS_DEFINE
};

}  // namespace taraxa
