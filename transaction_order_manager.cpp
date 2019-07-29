/*
 * @Copyright: Taraxa.io
 * @Author: Chia-Chun Lin
 * @Date: 2019-03-20 22:11:46
 * @Last Modified by: Chia-Chun Lin
 * @Last Modified time: 2019-03-20 22:11:46
 */
#include "transaction_order_manager.hpp"
#include "full_node.hpp"

namespace taraxa {

TransactionOrderManager::~TransactionOrderManager() {
  if (!stopped_) {
    stop();
  }
}
void TransactionOrderManager::start() {
  if (!stopped_) {
    return;
  }
  if (!node_.lock()) {
    LOG(log_er_) << "FullNode is not set ..." << std::endl;
    return;
  }
  db_trxs_to_blk_ = node_.lock()->getTrxsToBlkDB();
  stopped_ = false;
}
void TransactionOrderManager::stop() {
  if (stopped_) return;
  db_trxs_to_blk_ = nullptr;
  stopped_ = true;
}

std::vector<bool> TransactionOrderManager::computeOrderInBlock(
    DagBlock const& blk) {
  auto trxs = blk.getTrxs();
  std::vector<bool> res;
  for (auto const& t : trxs) {
    if (status_.get(t).second == false) {
      res.emplace_back(true);
      status_.insert(t, TransactionExecStatus::ordered);
      if (db_trxs_to_blk_) {
        auto ok = db_trxs_to_blk_->put(t.toString(), blk.getHash().toString());
        if (!ok) {
          LOG(log_er_) << "Cannot insert transaction " << t << " --> " << blk
                       << " mapping";
        }
      }
    } else {
      res.emplace_back(false);
    }
  }
  assert(trxs.size() == res.size());
  return res;
}

blk_hash_t TransactionOrderManager::getDagBlockFromTransaction(
    trx_hash_t const& trx) {
  auto blk = db_trxs_to_blk_->get(trx.toString());
  return blk_hash_t(blk);
}

std::shared_ptr<std::vector<TrxOverlapInBlock>>
TransactionOrderManager::computeOrderInBlocks(
    std::vector<std::shared_ptr<DagBlock>> const& blks) {
  auto ret = std::make_shared<std::vector<TrxOverlapInBlock>>();
  for (auto const& b : blks) {
    ret->emplace_back(std::make_pair(b->getHash(), computeOrderInBlock(*b)));
  }
  return ret;
}

}  // namespace taraxa
