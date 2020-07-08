#ifndef TARAXA_NODE_TRANSACTION_ORDER_MANAGER_HPP
#define TARAXA_NODE_TRANSACTION_ORDER_MANAGER_HPP

#pragma once

#include <libdevcore/Log.h>

#include <atomic>
#include <condition_variable>
#include <iostream>
#include <memory>
#include <set>
#include <thread>

#include "config.hpp"
#include "dag_block.hpp"
#include "db_storage.hpp"
#include "pbft_chain.hpp"
#include "types.hpp"
#include "util.hpp"

namespace taraxa {

class FullNode;

using TransactionOverlapTable =
    std::vector<std::pair<blk_hash_t, std::vector<bool>>>;
enum class TransactionExecStatus { non_ordered, ordered, executed };
using TransactionExecStatusTable =
    StatusTable<trx_hash_t, TransactionExecStatus>;
using TrxOverlapInBlock = std::pair<blk_hash_t, std::vector<bool>>;

class TransactionOrderManager {
 public:
  TransactionOrderManager(addr_t node_addr) { LOG_OBJECTS_CREATE("TRXORD"); }
  void setFullNode(std::shared_ptr<FullNode> node);
  void clear() { status_.clear(); }

  std::shared_ptr<std::vector<std::pair<blk_hash_t, std::vector<bool>>>>
  computeTransactionOverlapTable(std::shared_ptr<vec_blk_t> ordered_dag_blocks);
  std::vector<bool> computeOrderInBlock(
      DagBlock const& blk,
      TransactionExecStatusTable& status_for_proposing_blocks);
  std::shared_ptr<std::vector<TrxOverlapInBlock>> computeOrderInBlocks(
      std::vector<std::shared_ptr<DagBlock>> const& blks);
  std::shared_ptr<blk_hash_t> getDagBlockFromTransaction(trx_hash_t const& t);
  void updateOrderedTrx(TrxSchedule const& sche);

 private:
  std::atomic<bool> stopped_ = true;
  std::weak_ptr<FullNode> node_;
  TransactionExecStatusTable status_;
  std::shared_ptr<DbStorage> db_ = nullptr;
  LOG_OBJECTS_DEFINE;
};

}  // namespace taraxa

#endif
