/*
 * @Copyright: Taraxa.io
 * @Author: Chia-Chun Lin
 * @Date: 2019-01-24 16:56:41
 * @Last Modified by: Chia-Chun Lin
 * @Last Modified time: 2019-01-28 23:02:13
 */
#include "block_proposer.hpp"
#include "dag.hpp"
#include "full_node.hpp"
#include "transaction.hpp"
#include "types.hpp"
namespace taraxa {

std::atomic<uint64_t> BlockProposer::num_proposed_blocks = 0;

void BlockProposer::start() {
  if (!stopped_) return;
  if (full_node_.expired()) {
    LOG(log_er_) << "FullNode is not set ..." << std::endl;
    return;
  }
  LOG(log_nf_) << "BlockProposer threads = " << num_threads_ << std::endl;
  stopped_ = false;
  // reset number of proposed blocks
  BlockProposer::num_proposed_blocks = 0;
  if (!trx_mgr_.expired()) {
    trx_mgr_.lock()->start();
  } else {
    LOG(log_er_) << "Cannot start TransactionManager ..." << std::endl;
  }
  for (auto i = 0; i < num_threads_; ++i) {
    proposer_threads_.push_back(boost::thread([this]() { proposeBlock(); }));
  }
}

void BlockProposer::stop() {
  stopped_ = true;
  if (!trx_mgr_.expired()) {
    trx_mgr_.lock()->stop();
  }
  for (auto& t : proposer_threads_) {
    t.join();
  }
}
void BlockProposer::proposeBlock() {
  while (!stopped_) {
    std::string pivot;
    std::vector<std::string> tips;
    vec_trx_t to_be_packed_trx;
    if (trx_mgr_.expired()) {
      LOG(log_wr_) << "TransactionManager expired ..." << std::endl;
      break;
    }
    // prepare unpacked transaction
    // the call will block until ready
    trx_mgr_.lock()->packTrxs(to_be_packed_trx);
    if (to_be_packed_trx.empty()) {
      LOG(log_wr_) << "Skip block proposer, zero unpacked transactions ..."
                   << std::endl;
      continue;
    }
    if (dag_mgr_.expired()) {
      LOG(log_wr_) << "DagManager expired ..." << std::endl;
      break;
    }
    bool ok = dag_mgr_.lock()->getLatestPivotAndTips(pivot, tips);
    if (ok) {
      LOG(log_nf_) << "BlockProposer: pivot: " << pivot
                   << ", tip size = " << tips.size() << std::endl;
      LOG(log_nf_) << "Tips: " << std::endl;
      for (auto const& s : tips) {
        LOG(log_nf_) << s << std::endl;
      }
      LOG(log_nf_) << std::endl;
      BlockProposer::num_proposed_blocks.fetch_add(1);
    } else {
      LOG(log_wr_) << "Pivot and tips unavailable ..." << std::endl;
    }
    vec_blk_t tmp;
    for (auto const& t : tips) {
      tmp.emplace_back(blk_hash_t(t));
    }
    DagBlock blk(blk_hash_t(pivot), tmp, to_be_packed_trx);

    if (!full_node_.expired()) {
      LOG(log_nf_) << "Propose block" << std::endl;

      full_node_.lock()->storeBlockAndSign(blk);
    } else {
      LOG(log_er_) << "FullNode unavailable ..." << std::endl;
      return;
    }
  }
}

}  // namespace taraxa
