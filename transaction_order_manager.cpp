#include "transaction_order_manager.hpp"

#include "full_node.hpp"

namespace taraxa {

std::vector<bool> TransactionOrderManager::computeOrderInBlock(
    DagBlock const& blk,
    TransactionExecStatusTable& status_for_proposing_blocks) {
  auto trxs = blk.getTrxs();
  std::vector<bool> res;

  for (auto const& t : trxs) {
    if (status_.get(t).second == false &&
        status_for_proposing_blocks.get(t).second == false) {
      res.emplace_back(true);
      bool ok =
          status_for_proposing_blocks.insert(t, TransactionExecStatus::ordered);
      assert(ok);
    } else {
      res.emplace_back(false);
    }
  }
  assert(trxs.size() == res.size());
  return res;
}

void TransactionOrderManager::stop() {
  bool b = false; 
  stopped_.compare_exchange_strong(b, !b);
  blk_mgr_ = nullptr;
}

std::shared_ptr<blk_hash_t> TransactionOrderManager::getDagBlockFromTransaction(
    trx_hash_t const& trx) {
  return db_->getTransactionToBlock(trx);
}

void TransactionOrderManager::updateOrderedTrx(TrxSchedule const& sche) {
  for (auto i(0); i < sche.dag_blks_order.size(); ++i) {
    auto blk_hash = sche.dag_blks_order[i];
    auto dag_blk_trxs_mode = sche.trxs_mode[i];
    for (auto j(0); j < dag_blk_trxs_mode.size(); ++j) {
      auto const& trx_hash = dag_blk_trxs_mode[j].first;
      auto ok = status_.insert(trx_hash, TransactionExecStatus::ordered);
      if (!ok) {
        LOG(log_er_) << "Transaction " << trx_hash << " has been executed";
      }
      if (db_) {
        auto exists = db_->transactionToBlockInDb(trx_hash);
        if (!exists) {
          db_->saveTransactionToBlock(trx_hash, blk_hash);
        } else {
          LOG(log_er_) << "Cannot insert transaction " << trx_hash << " --> "
                       << blk_hash << " mapping, it has been executed in blk "
                       << db_->getTransactionToBlock(trx_hash);
        }
      }
    }
  }
}

std::shared_ptr<std::vector<std::pair<blk_hash_t, std::vector<bool>>>>
TransactionOrderManager::computeTransactionOverlapTable(
    std::shared_ptr<vec_blk_t> ordered_dag_blocks) {
  std::vector<std::shared_ptr<DagBlock>> blks;
  for (auto const& b : *ordered_dag_blocks) {
    auto dagblk = blk_mgr_->getDagBlock(b);
    assert(dagblk);
    blks.emplace_back(dagblk);
  }
  return computeOrderInBlocks(blks);
}

std::shared_ptr<std::vector<TrxOverlapInBlock>>
TransactionOrderManager::computeOrderInBlocks(
    std::vector<std::shared_ptr<DagBlock>> const& blks) {
  auto ret = std::make_shared<std::vector<TrxOverlapInBlock>>();
  TransactionExecStatusTable
      status_for_proposing_blocks;  // this proposal may not be confirmed
  for (auto const& b : blks) {
    auto order = computeOrderInBlock(*b, status_for_proposing_blocks);
    ret->emplace_back(std::make_pair(b->getHash(), order));
  }
  return ret;
}

}  // namespace taraxa
