/*
 * @Copyright: Taraxa.io
 * @Author: Chia-Chun Lin
 * @Date: 2019-01-24 16:56:41
 * @Last Modified by: Chia-Chun Lin
 * @Last Modified time: 2019-01-28 23:02:13
 */

#include "block_proposer.hpp"
#include "dag.hpp"

namespace taraxa {

std::atomic<uint64_t> BlockProposer::num_proposed_blocks = 0;

uint64_t BlockProposer::getNumProposedBlocks() {
  return BlockProposer::num_proposed_blocks;
}

BlockProposer::BlockProposer(unsigned num_threads,
                             std::shared_ptr<DagManager> dag_mgr)
    : num_threads_(num_threads), dag_mgr_(dag_mgr) {}

BlockProposer::~BlockProposer() {
  if (!stopped_) stop();
}

void BlockProposer::setVerbose(bool verbose) { verbose_ = verbose; }

void BlockProposer::start() {
  if (verbose_) {
    std::cout << "BlockProposer threads = " << num_threads_ << std::endl;
  }
  if (!stopped_) return;
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
    bool ok = dag_mgr_->getLatestPivotAndTips(pivot, tips);
    if (verbose_ && ok) {
      std::cout << "BlockProposer: \nPivot: " << pivot << std::endl;
      std::cout << "Tips: " << std::endl;
      for (auto const& s : tips) {
        std::cout << s << std::endl;
      }
      std::cout << std::endl;
    }
    if (ok) {
      BlockProposer::num_proposed_blocks.fetch_add(1);
    }
  }
}

}  // namespace taraxa
