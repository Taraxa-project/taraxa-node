#include "block_proposer.hpp"
#include <cmath>
#include "dag.hpp"
#include "full_node.hpp"
#include "transaction.hpp"
#include "types.hpp"

namespace taraxa {

std::atomic<uint64_t> BlockProposer::num_proposed_blocks = 0;
std::mt19937 RandomPropose::generator;
using namespace vdf_sortition;

uint posLog2(val_t val) {
  if (val <= 0) return 0;
  int count = 0;
  while (val) {
    val >>= 1;
    count += 1;
  }
  return count - 1;
}

bool RandomPropose::propose() {
  auto proposer = proposer_.lock();
  if (!proposer) {
    LOG(log_er_) << "Block proposer not available" << std::endl;
    return false;
  }
  auto delay = distribution_(RandomPropose::generator);
  thisThreadSleepForMilliSeconds(delay);
  LOG(log_tr_) << "Add proposer delay " << delay << std::endl;

  vec_trx_t sharded_trxs;
  DagFrontier frontier;

  bool ok = proposer->getShardedTrxs(sharded_trxs, frontier);
  if (!ok) {
    return false;
  }
  assert(!frontier.pivot.isZero());
  auto propose_level =
      proposer->getProposeLevel(frontier.pivot, frontier.tips) + 1;

  DagBlock blk(frontier.pivot, propose_level, frontier.tips, sharded_trxs);

  proposer_.lock()->proposeBlock(blk);
  return true;
}

bool SortitionPropose::propose() {
  auto proposer = proposer_.lock();
  bool ret = false;
  if (!proposer) {
    LOG(log_er_) << "Block proposer not available" << std::endl;
    return false;
  }
  thisThreadSleepForMilliSeconds(min_propose_delay);

  vec_trx_t sharded_trxs;
  DagFrontier frontier;

  bool ok = proposer->getShardedTrxs(sharded_trxs, frontier);
  if (!ok) {
    return false;
  }
  assert(!frontier.pivot.isZero());

  auto propose_level =
      proposer->getProposeLevel(frontier.pivot, frontier.tips) + 1;
  if (propose_level <= last_proposed_level_) {
    return false;
  }

  auto latest_anchor = proposer->getLatestAnchor();

  // get sortition
  vdf_sortition::VdfMsg vdf_msg(latest_anchor, propose_level);
  vdf_sortition::VdfSortition vdf(vrf_sk_, vdf_msg, difficulty_bound_,
                                  lambda_bits_);
  vdf.computeVdfSolution();
  assert(vdf.verify());
  LOG(log_nf_) << "VDF computation time " << vdf.getComputationTime()
               << " difficulty " << vdf.getDifficulty();
  DagBlock blk(frontier.pivot, propose_level, frontier.tips, sharded_trxs, vdf);

  proposer->proposeBlock(blk);
  last_proposed_level_ = propose_level;
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
  if (bool b = true; !stopped_.compare_exchange_strong(b, !b)) {
    return;
  }
  LOG(log_nf_) << "BlockProposer started ..." << std::endl;
  // reset number of proposed blocks
  BlockProposer::num_proposed_blocks = 0;
  propose_model_->setProposer(getShared(), full_node_.lock()->getSecretKey(),
                              full_node_.lock()->getVrfSecretKey());
  proposer_worker_ = std::make_shared<std::thread>([this]() {
    while (!stopped_) {
      propose_model_->propose();
    }
  });
}

void BlockProposer::stop() {
  if (bool b = false; !stopped_.compare_exchange_strong(b, !b)) {
    return;
  }
  proposer_worker_->join();
}

void BlockProposer::setFullNode(std::shared_ptr<FullNode> full_node) {
  full_node_ = full_node;
  auto addr = std::stoull(
      full_node->getAddress().toString().substr(0, 6).c_str(), NULL, 16);
  my_trx_shard_ = addr % conf_.shard;
  LOG(log_nf_) << "Block proposer in " << my_trx_shard_ << " shard ...";
}
level_t BlockProposer::getMaxDagLevel() const {
  auto full_node = full_node_.lock();
  if (full_node) {
    return full_node->getMaxDagLevel();
  }
  return 0;
}
bool BlockProposer::getLatestPivotAndTips(blk_hash_t& pivot, vec_blk_t& tips) {
  std::string pivot_string;
  std::vector<std::string> tips_string;
  auto dag_mgr = dag_mgr_.lock();
  if (!dag_mgr) {
    LOG(log_wr_) << "DagManager expired ..." << std::endl;
    return false;
  }
  bool ok = dag_mgr->getLatestPivotAndTips(pivot_string, tips_string);
  if (ok) {
    LOG(log_nf_) << "BlockProposer: pivot: " << pivot.toString()
                 << ", tip size = " << tips.size() << std::endl;
    LOG(log_tr_) << "Tips: " << tips;
  } else {
    LOG(log_er_) << "Pivot and tips unavailable ..." << std::endl;
    return ok;
  }
  auto& log_time = full_node_.lock()->getTimeLogger();

  LOG(log_time) << "Pivot and Tips retrieved at: "
                << getCurrentTimeMilliSeconds();
  pivot = blk_hash_t(pivot_string);
  tips.clear();
  for (auto const& t : tips_string) {
    tips.emplace_back(blk_hash_t(t));
  }
  return ok;
}

bool BlockProposer::getShardedTrxs(uint total_shard, DagFrontier& frontier,
                                   uint my_shard, vec_trx_t& sharded_trxs) {
  auto full_node = full_node_.lock();
  auto& log_time = full_node->getTimeLogger();
  vec_trx_t to_be_packed_trx;
  auto trx_mgr = trx_mgr_.lock();
  if (!trx_mgr) {
    LOG(log_er_) << "TransactionManager expired ..." << std::endl;
    return false;
  }
  trx_mgr->packTrxs(to_be_packed_trx, frontier);
  if (to_be_packed_trx.empty()) {
    LOG(log_tr_) << "Skip block proposer, zero unpacked transactions ..."
                 << std::endl;
    return false;
  }
  sharded_trxs.clear();
  for (auto const& t : to_be_packed_trx) {
    auto shard = std::stoull(t.toString().substr(0, 10), NULL, 16);
    if (shard % total_trx_shards_ == my_trx_shard_) {
      sharded_trxs.emplace_back(t);
    }
  }
  if (sharded_trxs.empty()) {
    LOG(log_tr_) << "Skip block proposer, zero sharded transactions ..."
                 << std::endl;
    return false;
  }
  return true;
}

blk_hash_t BlockProposer::getLatestAnchor() const {
  return full_node_.lock()->getLatestAnchor();
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

void BlockProposer::proposeBlock(DagBlock& blk) {
  auto full_node = full_node_.lock();
  assert(full_node);

  // Blocks are not proposed if we are behind the network and still syncing
  if (!full_node->isSynced()) {
    return;
  }

  blk.sign(full_node->getSecretKey());
  full_node_.lock()->insertBlock(blk);

  auto& log_time = full_node->getTimeLogger();
  auto now = getCurrentTimeMilliSeconds();

  LOG(log_time) << "Propose block " << blk.getHash() << " at: " << now
                << " ,trxs: " << blk.getTrxs()
                << " , tips: " << blk.getTips().size();
  LOG(log_si_) << getFullNodeAddress() << " Propose block :" << blk.getHash()
               << " pivot: " << blk.getPivot() << " , number of trx ("
               << blk.getTrxs().size() << ")";
  BlockProposer::num_proposed_blocks.fetch_add(1);
}

addr_t BlockProposer::getFullNodeAddress() const {
  auto full_node = full_node_.lock();
  if (full_node) {
    return full_node->getAddress();
  } else {
    return addr_t();
  }
}

}  // namespace taraxa
