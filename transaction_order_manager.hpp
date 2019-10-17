#ifndef TARAXA_NODE_TRANSACTION_ORDER_MANAGER_HPP
#define TARAXA_NODE_TRANSACTION_ORDER_MANAGER_HPP

#pragma once

#include <atomic>
#include <condition_variable>
#include <iostream>
#include <memory>
#include <set>
#include <thread>
#include "dag_block.hpp"
#include "libdevcore/Log.h"
#include "pbft_chain.hpp"
#include "simple_db_face.hpp"
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

// TODO: the table need to flush out
// Compute 1) transaction order and 2) map[transaction --> dagblock]
class TransactionOrderManager {
 public:
  TransactionOrderManager() = default;
  ~TransactionOrderManager();
  void start();
  void setFullNode(std::shared_ptr<FullNode> node) { node_ = node; }
  void stop();
  void clear() { status_.clear(); }

  std::vector<bool> computeOrderInBlock(
      DagBlock const& blk,
      TransactionExecStatusTable& status_for_proposing_blocks);
  std::shared_ptr<std::vector<TrxOverlapInBlock>> computeOrderInBlocks(
      std::vector<std::shared_ptr<DagBlock>> const& blks);
  blk_hash_t getDagBlockFromTransaction(trx_hash_t const& t);
  bool updateOrderedTrx(TrxSchedule const& sche);

 private:
  std::atomic<bool> stopped_ = true;
  std::weak_ptr<FullNode> node_;
  TransactionExecStatusTable status_;
  std::shared_ptr<SimpleDBFace> db_trxs_to_blk_ = nullptr;
  std::shared_ptr<SimpleDBFace> db_blks_ = nullptr;
  dev::Logger log_er_{
      dev::createLogger(dev::Verbosity::VerbosityError, "TRXODR")};
  dev::Logger log_wr_{
      dev::createLogger(dev::Verbosity::VerbosityWarning, "TRXODR")};
  dev::Logger log_nf_{
      dev::createLogger(dev::Verbosity::VerbosityInfo, "TRXODR")};
};

}  // namespace taraxa

#endif
