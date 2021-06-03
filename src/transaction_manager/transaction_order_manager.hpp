#pragma once

#include <atomic>
#include <condition_variable>
#include <iostream>
#include <memory>
#include <set>
#include <thread>

#include "common/types.hpp"
#include "config/config.hpp"
#include "consensus/pbft_chain.hpp"
#include "dag/dag_block.hpp"
#include "storage/db_storage.hpp"
#include "util/util.hpp"

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
  std::shared_ptr<std::vector<TrxOverlapInBlock>> computeOrderInBlocks(
      std::vector<std::shared_ptr<DagBlock>> const& blks);

 private:
  TransactionExecStatusTable status_;
  std::shared_ptr<DbStorage> db_;
  LOG_OBJECTS_DEFINE
};

}  // namespace taraxa
