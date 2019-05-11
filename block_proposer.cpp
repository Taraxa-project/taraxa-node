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
std::mt19937 RandomPropose::generator;

bool RandomPropose::propose() {
  auto delay = distribution_(RandomPropose::generator);
  thisThreadSleepForMilliSeconds(delay);
  LOG(log_tr_) << "Add proposer delay " << delay << std::endl;
  if (proposer_.expired()) {
    LOG(log_er_) << "Block proposer not available" << std::endl;
    return false;
  }
  proposer_.lock()->proposeBlock();
  return true;
}

std::shared_ptr<BlockProposer> BlockProposer::getShared() {
  try {
    return shared_from_this();
  } catch (std::bad_weak_ptr& e) {
    std::cerr << "FullNode: " << e.what() << std::endl;
    return nullptr;
  }
}

void BlockProposer::start() {
  if (!stopped_) return;
  if (full_node_.expired()) {
    LOG(log_er_) << "FullNode is not set ..." << std::endl;
    return;
  }

  if (!trx_mgr_.expired()) {
    trx_mgr_.lock()->start();
  } else {
    LOG(log_er_) << "Cannot start TransactionManager ..." << std::endl;
    return;
  }
  LOG(log_nf_) << "BlockProposer started ..." << std::endl;
  stopped_ = false;
  // reset number of proposed blocks
  BlockProposer::num_proposed_blocks = 0;
  propose_model_->setProposer(getShared());
  proposer_worker_ = std::make_shared<std::thread>([this]() {
    while (!stopped_) {
      propose_model_->propose();
    }
  });
}
void BlockProposer::stop() {
  if (stopped_) return;
  stopped_ = true;
  proposer_worker_->join();
}

void BlockProposer::setFullNode(std::shared_ptr<FullNode> full_node) {
  full_node_ = full_node;
  auto addr = std::stoull(
      full_node->getAddress().toString().substr(0, 6).c_str(), NULL, 16);
  ith_shard_ = addr % conf_.shards;
  LOG(log_nf_) << "Block proposer in " << ith_shard_ << " shard ...";
}
void BlockProposer::proposeBlock() {
  std::string pivot;
  std::vector<std::string> tips;
  vec_trx_t to_be_packed_trx;
  if (trx_mgr_.expired()) {
    LOG(log_wr_) << "TransactionManager expired ..." << std::endl;
    return;
  }
  auto& log_time = full_node_.lock()->getTimeLogger();

  trx_mgr_.lock()->packTrxs(to_be_packed_trx);
  if (to_be_packed_trx.empty()) {
    LOG(log_tr_) << "Skip block proposer, zero unpacked transactions ..."
                 << std::endl;
    return;
  }

  vec_trx_t sharded_trxs;
  for (auto const& t : to_be_packed_trx) {
    auto shard = std::stoull(t.toString().substr(0, 10), NULL, 16);
    if (shard % num_shards_ == ith_shard_) {
      sharded_trxs.emplace_back(t);
    }
  }
  if (sharded_trxs.empty()) {
    LOG(log_tr_) << "Skip block proposer, zero sharded transactions ..."
                 << std::endl;
    return;
  }
  if (dag_mgr_.expired()) {
    LOG(log_wr_) << "DagManager expired ..." << std::endl;
    return;
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
  LOG(log_time) << "Pivot and Tips retrieved at: "
                << getCurrentTimeMilliSeconds();

  vec_blk_t tmp;
  for (auto const& t : tips) {
    tmp.emplace_back(blk_hash_t(t));
  }
  DagBlock blk(blk_hash_t(pivot), tmp, sharded_trxs);
  if (!full_node_.expired()) {
    LOG(log_nf_) << "Propose block" << std::endl;

    full_node_.lock()->storeBlockAndSign(blk);
  } else {
    LOG(log_er_) << "FullNode unavailable ..." << std::endl;
    return;
  }
}

}  // namespace taraxa
