#include "transaction_order_manager.hpp"
#include "full_node.hpp"

namespace taraxa {

TransactionOrderManager::~TransactionOrderManager() { stop(); }

void TransactionOrderManager::start() {
  if (bool b = true; !stopped_.compare_exchange_strong(b, !b)) {
    return;
  }
  if (!node_.lock()) {
    LOG(log_er_) << "FullNode is not set ..." << std::endl;
    return;
  }
  db_trxs_to_blk_ = node_.lock()->getTrxsToBlkDB();
  db_blks_ = node_.lock()->getBlksDB();
}

void TransactionOrderManager::stop() {
  if (bool b = false; !stopped_.compare_exchange_strong(b, !b)) {
    return;
  }
  db_trxs_to_blk_ = nullptr;
  db_blks_ = nullptr;
}

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

blk_hash_t TransactionOrderManager::getDagBlockFromTransaction(
    trx_hash_t const& trx) {
  auto blk = db_trxs_to_blk_->lookup(trx.toString());
  return blk_hash_t(blk);
}

bool TransactionOrderManager::updateOrderedTrx(TrxSchedule const& sche) {
  for (auto i(0); i < sche.blk_order.size(); ++i) {
    auto blk = sche.blk_order[i];
    auto trx_modes = sche.vec_trx_modes[i];
    auto blk_bytes = db_blks_->get(blk);
    if (blk_bytes.size() == 0) {
      LOG(log_er_) << "Cannot get block from db: " << blk << std::endl;
      return false;
    }
    DagBlock dag_block(blk_bytes);
    auto trxs_hash = dag_block.getTrxs();

    for (auto i(0); i < trxs_hash.size(); ++i) {
      auto const& trx_hash = trxs_hash[i];
      auto mode = trx_modes[i];
      if (mode == 0) {
        continue;
      } else {
        auto ok = status_.insert(trx_hash, TransactionExecStatus::ordered);
        if (!ok) {
          LOG(log_er_) << "Transaction " << trx_hash << " has been executed";
        }
        if (db_trxs_to_blk_) {
          auto exists = db_trxs_to_blk_->exists(trx_hash.toString());
          if (!exists) {
            db_trxs_to_blk_->insert(trx_hash.toString(), blk.toString());
          } else {
            LOG(log_er_) << "Cannot insert transaction " << trx_hash << " --> "
                         << blk << " mapping, it has been executed in blk "
                         << db_trxs_to_blk_->lookup(trx_hash.toString());
          }
        }
      }
    }
  }
  return true;
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
