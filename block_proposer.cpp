#include "block_proposer.hpp"

#include <cmath>

#include "dag.hpp"
#include "transaction.hpp"
#include "transaction_manager.hpp"

namespace taraxa {

using namespace vdf_sortition;
std::atomic<uint64_t> BlockProposer::num_proposed_blocks = 0;

bool SortitionPropose::propose() {
  auto proposer = proposer_.lock();
  if (!proposer) {
    LOG(log_er_) << "Block proposer not available" << std::endl;
    return false;
  }

  auto dag_height = dag_mgr_->getMaxLevel();
  if (dag_height == last_dag_height_ && last_dag_height_ > 0) {
    LOG(log_tr_) << "Skip proposing, dag not increasing, height "
                 << last_dag_height_;
    return false;
  }

  vec_trx_t sharded_trxs;
  DagFrontier frontier;
  bool ok = proposer->getShardedTrxs(sharded_trxs, frontier);
  if (!ok) {
    return false;
  }
  assert(!frontier.pivot.isZero());
  auto propose_level =
      proposer->getProposeLevel(frontier.pivot, frontier.tips) + 1;

  // get sortition
  vdf_sortition::Message msg(propose_level);
  vdf_sortition::VdfSortition vdf(node_addr_, vrf_sk_, msg, difficulty_bound_,
                                  lambda_bound_);
  vdf.computeVdfSolution(frontier.pivot.toString());
  LOG(log_nf_) << "VDF computation time " << vdf.getComputationTime()
               << " difficulty " << vdf.getDifficulty();
  DagBlock blk(frontier.pivot, propose_level, frontier.tips, sharded_trxs, vdf);
  proposer->proposeBlock(blk);
  last_dag_height_ = dag_height;

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
  propose_model_->setProposer(getShared(), node_addr_, node_sk_, vrf_sk_);
  // reset number of proposed blocks
  BlockProposer::num_proposed_blocks = 0;
  proposer_worker_ = std::make_shared<std::thread>([this]() {
    while (!stopped_) {
      // Blocks are not proposed if we are behind the network and still syncing
      if (!network_->isSynced()) {
        continue;
      }
      propose_model_->propose();
      thisThreadSleepForMilliSeconds(min_proposal_delay);
    }
  });
}

void BlockProposer::stop() {
  if (bool b = false; !stopped_.compare_exchange_strong(b, !b)) {
    return;
  }
  proposer_worker_->join();
}

bool BlockProposer::getLatestPivotAndTips(blk_hash_t& pivot, vec_blk_t& tips) {
  std::string pivot_string;
  std::vector<std::string> tips_string;
  bool ok = dag_mgr_->getLatestPivotAndTips(pivot_string, tips_string);
  if (ok) {
    LOG(log_nf_) << "BlockProposer: pivot: " << pivot.toString()
                 << ", tip size = " << tips.size() << std::endl;
    LOG(log_tr_) << "Tips: " << tips;
  } else {
    LOG(log_er_) << "Pivot and tips unavailable ..." << std::endl;
    return ok;
  }

  LOG(log_time_) << "Pivot and Tips retrieved at: "
                 << getCurrentTimeMilliSeconds();
  pivot = blk_hash_t(pivot_string);
  tips.clear();
  std::transform(tips_string.begin(), tips_string.end(),
                 std::back_inserter(tips),
                 [](const std::string& item) { return blk_hash_t(item); });
  return ok;
}

bool BlockProposer::getShardedTrxs(uint total_shard, DagFrontier& frontier,
                                   uint my_shard, vec_trx_t& sharded_trxs) {
  vec_trx_t to_be_packed_trx;
  trx_mgr_->packTrxs(to_be_packed_trx, frontier, conf_.transaction_limit);
  // Need to update pivot incase a new period is confirmed
  std::vector<std::string> ghost;
  if (dag_mgr_) {
    dag_mgr_->getGhostPath(ghost);
    vec_blk_t gg;
    std::transform(ghost.begin(), ghost.end(), std::back_inserter(gg),
                   [](std::string const& t) { return blk_hash_t(t); });
    for (auto const& g : gg) {
      if (g == frontier.pivot) {  // pivot does not change
        break;
      }
      auto iter = std::find(frontier.tips.begin(), frontier.tips.end(), g);
      if (iter != std::end(frontier.tips)) {
        std::swap(frontier.pivot, *iter);
        LOG(log_si_) << " Swap frontier with pivot: " << *iter
                     << " tips: " << frontier.pivot;
      }
    }
  }

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

blk_hash_t BlockProposer::getProposeAnchor() const {
  auto anchors = dag_mgr_->getAnchors();
  if (anchors.first == "") {
    // Only includes DAG genesis
    return blk_hash_t(anchors.second);
  } else {
    // return second to last anchor
    return blk_hash_t(anchors.first);
  }
}

level_t BlockProposer::getProposeLevel(blk_hash_t const& pivot,
                                       vec_blk_t const& tips) {
  level_t max_level = 0;
  // get current level
  auto pivot_blk = blk_mgr_->getDagBlock(pivot);
  if (!pivot_blk) {
    LOG(log_er_) << "Cannot find pivot dag block " << pivot;
    return 0;
  }
  max_level = std::max(pivot_blk->getLevel(), max_level);

  for (auto const& t : tips) {
    auto tip_blk = blk_mgr_->getDagBlock(t);
    if (!tip_blk) {
      LOG(log_er_) << "Cannot find tip dag block " << blk_hash_t(t);
      return 0;
    }
    max_level = std::max(tip_blk->getLevel(), max_level);
  }
  return max_level;
}

void BlockProposer::proposeBlock(DagBlock& blk) {
  if (stopped_) return;

  blk.sign(node_sk_);
  blk_mgr_->insertBlock(blk);

  auto now = getCurrentTimeMilliSeconds();
  LOG(log_time_) << "Propose block " << blk.getHash() << " at: " << now
                 << " ,trxs: " << blk.getTrxs()
                 << " , tips: " << blk.getTips().size();
  LOG(log_nf_) << " Propose block :" << blk.getHash()
               << " pivot: " << blk.getPivot() << " , number of trx ("
               << blk.getTrxs().size() << ")";
  BlockProposer::num_proposed_blocks.fetch_add(1);
}

}  // namespace taraxa
