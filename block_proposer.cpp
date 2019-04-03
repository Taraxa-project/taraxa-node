/*
 * @Copyright: Taraxa.io
 * @Author: Chia-Chun Lin
 * @Date: 2019-01-24 16:56:41
 * @Last Modified by: Chia-Chun Lin
 * @Last Modified time: 2019-01-28 23:02:13
 */
#include "block_proposer.hpp"
#include "dag.hpp"
#include "transaction.hpp"

namespace taraxa {

std::atomic<uint64_t> BlockProposer::num_proposed_blocks = 0;

void BlockProposer::start() {
  if (!stopped_) return;
  LOG(logger_) << "BlockProposer threads = " << num_threads_ << std::endl;
  stopped_ = false;
  for (auto i = 0; i < num_threads_; ++i) {
    proposer_threads_.push_back(boost::thread([this]() { proposeBlock(); }));
  }
}

void BlockProposer::stop() {
  stopped_ = true;
  for (auto& t : proposer_threads_) {
    t.join();
  }
}
void BlockProposer::proposeBlock() {
  while (!stopped_) {
    std::string pivot;
    std::vector<std::string> tips;
    vec_trx_t to_be_packed_trx;
    if (!trx_mgr_.lock()) {
      LOG(logger_) << "TransactionManager expired ..." << std::endl;
      break;
    }
    // prepare unpacked transaction
    // the call will block until ready
    trx_mgr_.lock()->packTrxs(to_be_packed_trx);
    if (to_be_packed_trx.empty()) {
      LOG(logger_) << "Skip block proposer, zero unpacked transactions ..."
                   << std::endl;
      continue;
    }
    if (!dag_mgr_.lock()) {
      LOG(logger_) << "DagManager expired ..." << std::endl;
      break;
    }
    bool ok = dag_mgr_.lock()->getLatestPivotAndTips(pivot, tips);
    if (ok) {
      LOG(logger_) << "BlockProposer: \nPivot: " << pivot << std::endl;
      LOG(logger_) << "Tips: " << std::endl;
      for (auto const& s : tips) {
        LOG(logger_) << s << std::endl;
      }
      LOG(logger_) << std::endl;
    }
    if (ok) {
      BlockProposer::num_proposed_blocks.fetch_add(1);
    }
  }
}

}  // namespace taraxa
