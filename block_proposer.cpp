/*
 * @Copyright: Taraxa.io
 * @Author: Chia-Chun Lin
 * @Date: 2019-01-24 16:56:41
 * @Last Modified by: Chia-Chun Lin
 * @Last Modified time: 2019-05-16 12:28:12
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

bool SortitionPropose::propose() {
  thisThreadSleepForMilliSeconds(propose_interval_);
  auto proposer = proposer_.lock();
  if (!proposer) {
    LOG(log_er_) << "Block proposer not available" << std::endl;
    return false;
  }
  std::string pivot;
  std::vector<std::string> tips;

  bool ok = proposer->getLatestPivotAndTips(pivot, tips);
  if (ok) {
    LOG(log_nf_) << "BlockProposer: pivot: " << pivot
                 << ", tip size = " << tips.size() << std::endl;
    LOG(log_nf_) << "Tips: " << tips;
  } else {
    LOG(log_er_) << "Pivot and tips unavailable ..." << std::endl;
    return false;
  }
  vec_blk_t tip_hashes;
  for (auto const& t : tips) {
    tip_hashes.emplace_back(blk_hash_t(t));
  }
  auto pivot_hash = blk_hash_t(pivot);
  auto propose_level = proposer->getProposeLevel(pivot_hash, tip_hashes) + 1;

  // get sortition

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

  LOG(log_nf_) << "BlockProposer started ..." << std::endl;
  stopped_ = false;
  // reset number of proposed blocks
  BlockProposer::num_proposed_blocks = 0;
  propose_model_->setProposer(getShared(), full_node_.lock()->getSecretKey());
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

bool BlockProposer::getLatestPivotAndTips(std::string pivot,
                                          std::vector<std::string> tips) {
  bool ok = dag_mgr_.lock()->getLatestPivotAndTips(pivot, tips);
  if (ok) {
    LOG(log_nf_) << "BlockProposer: pivot: " << pivot
                 << ", tip size = " << tips.size() << std::endl;
    LOG(log_nf_) << "Tips: " << tips;
  } else {
    LOG(log_er_) << "Pivot and tips unavailable ..." << std::endl;
    return ok;
  }
  auto& log_time = full_node_.lock()->getTimeLogger();

  LOG(log_time) << "Pivot and Tips retrieved at: "
                << getCurrentTimeMilliSeconds();
  return ok;
}

level_t BlockProposer::getProposeLevel(blk_hash_t const& pivot,
                                       vec_blk_t const& tips) {
  level_t max_level = 0;
  auto full_node = full_node_.lock();
  if (!full_node) {
    LOG(log_er_) << "FullNode expired, cannot get propose level ..."
                 << std::endl;
    return 0;
  }
  // get current level
  auto pivot_blk = full_node->getDagBlock(pivot);
  if (!pivot_blk) {
    LOG(log_er_) << "Cannot find pivot dag block " << pivot;
    return 0;
  }
  max_level = std::max(pivot_blk->getLevel(), max_level);

  for (auto const& t : tips) {
    auto tip_blk = full_node->getDagBlock(t);
    if (!tip_blk) {
      LOG(log_er_) << "Cannot find tip dag block " << blk_hash_t(t);
      return 0;
    }
    max_level = std::max(tip_blk->getLevel(), max_level);
  }
  return max_level;
}

void BlockProposer::proposeBlock() {
  std::string pivot;
  std::vector<std::string> tips;
  vec_trx_t to_be_packed_trx;
  if (trx_mgr_.expired()) {
    LOG(log_er_) << "TransactionManager expired ..." << std::endl;
    return;
  }
  auto full_node = full_node_.lock();
  if (!full_node) {
    LOG(log_er_) << "FullNode expired ..." << std::endl;
    return;
  }
  auto& log_time = full_node->getTimeLogger();

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
  bool ok = getLatestPivotAndTips(pivot, tips);
  if (ok) {
    LOG(log_nf_) << "BlockProposer: pivot: " << pivot
                 << ", tip size = " << tips.size() << std::endl;
    LOG(log_nf_) << "Tips: " << tips;
  } else {
    LOG(log_er_) << "Pivot and tips unavailable ..." << std::endl;
    return;
  }
  LOG(log_time) << "Pivot and Tips retrieved at: "
                << getCurrentTimeMilliSeconds();

  vec_blk_t tip_hashes;
  for (auto const& t : tips) {
    tip_hashes.emplace_back(blk_hash_t(t));
  }
  auto pivot_hash = blk_hash_t(pivot);
  auto propose_level = getProposeLevel(pivot_hash, tip_hashes) + 1;

  LOG(log_nf_) << "Propose block" << std::endl;
  DagBlock blk(pivot_hash, propose_level, tip_hashes, sharded_trxs);
  assert(sharded_trxs.size());
  full_node->insertBlockAndSign(blk);
  BlockProposer::num_proposed_blocks.fetch_add(1);
}

}  // namespace taraxa
