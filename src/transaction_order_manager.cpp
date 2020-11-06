#include "transaction_order_manager.hpp"

namespace taraxa {

std::vector<bool> TransactionOrderManager::computeOrderInBlock(DagBlock const& blk, TransactionExecStatusTable& status_for_proposing_blocks) {
  auto trxs = blk.getTrxs();
  std::vector<bool> res;

  for (auto const& t : trxs) {
    if (status_.get(t).second == false && status_for_proposing_blocks.get(t).second == false) {
      res.emplace_back(true);
      bool ok = status_for_proposing_blocks.insert(t, TransactionExecStatus::ordered);
      assert(ok);
    } else {
      res.emplace_back(false);
    }
  }
  assert(trxs.size() == res.size());
  return res;
}

std::shared_ptr<std::vector<TrxOverlapInBlock>> TransactionOrderManager::computeOrderInBlocks(std::vector<std::shared_ptr<DagBlock>> const& blks) {
  auto ret = std::make_shared<std::vector<TrxOverlapInBlock>>();
  TransactionExecStatusTable status_for_proposing_blocks;  // this proposal may not be confirmed
  for (auto const& b : blks) {
    auto order = computeOrderInBlock(*b, status_for_proposing_blocks);
    ret->emplace_back(std::make_pair(b->getHash(), order));
  }
  return ret;
}

}  // namespace taraxa
