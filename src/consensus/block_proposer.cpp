#include "block_proposer.hpp"

#include <cmath>

#include "dag/dag.hpp"
#include "transaction_manager/transaction.hpp"
#include "transaction_manager/transaction_manager.hpp"
#include "util/util.hpp"

namespace taraxa {

using namespace vdf_sortition;
std::atomic<uint64_t> BlockProposer::num_proposed_blocks = 0;

bool SortitionPropose::propose() {
  auto proposer = proposer_.lock();
  if (!proposer) {
    LOG(log_er_) << "Block proposer not available" << std::endl;
    return false;
  }

  if (trx_mgr_->getTransactionQueueSize().second == 0) {
    return false;
  }

  DagFrontier frontier = dag_mgr_->getDagFrontier();
  LOG(log_dg_) << "Get frontier with pivot: " << frontier.pivot << " tips: " << frontier.tips;
  assert(!frontier.pivot.isZero());
  auto propose_level = proposer->getProposeLevel(frontier.pivot, frontier.tips) + 1;
  if (!proposer->validDposProposer(propose_level)) {
    LOG(log_tr_) << node_addr_ << " is not a valid DPOS proposer for DAG level " << propose_level;
    return false;
  }

  // get sortition
  vdf_sortition::VdfSortition vdf(vdf_config_, node_addr_, vrf_sk_, getRlpBytes(propose_level));
  if (vdf.isStale(vdf_config_)) {
    if (propose_level == last_propose_level_ && num_tries_ < max_num_tries_) {
      LOG(log_dg_) << "Will not propose DAG block. Get difficulty at stale, last propose level " << last_propose_level_
                   << ", has tried " << num_tries_ << " times.";
      num_tries_++;
      return false;
    } else if (propose_level != last_propose_level_) {
      LOG(log_dg_) << "Will not propose DAG block, will reset number of tries. "
                      "Get difficulty at stale, last propose level "
                   << last_propose_level_ << ", current propose level " << propose_level;
      last_propose_level_ = propose_level;
      num_tries_ = 0;
      return false;
    }
  }
  vdf.computeVdfSolution(vdf_config_, frontier.pivot.asBytes());
  if (vdf.isStale(vdf_config_)) {
    DagFrontier latestFrontier = dag_mgr_->getDagFrontier();
    if (latestFrontier.pivot != frontier.pivot) return false;
  }

  vec_trx_t sharded_trxs;
  bool ok = proposer->getShardedTrxs(sharded_trxs);
  if (!ok) {
    return false;
  }
  LOG(log_nf_) << "VDF computation time " << vdf.getComputationTime() << " difficulty " << vdf.getDifficulty();

  DagBlock blk(frontier.pivot, propose_level, frontier.tips, sharded_trxs, vdf);
  proposer->proposeBlock(blk);

  last_propose_level_ = propose_level;
  num_tries_ = 0;
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
    LOG(log_nf_) << "BlockProposer: pivot: " << pivot.toString() << ", tip size = " << tips.size() << std::endl;
    LOG(log_tr_) << "Tips: " << tips;
  } else {
    LOG(log_er_) << "Pivot and tips unavailable ..." << std::endl;
    return ok;
  }

  LOG(log_time_) << "Pivot and Tips retrieved at: " << getCurrentTimeMilliSeconds();
  pivot = blk_hash_t(pivot_string);
  tips.clear();
  std::transform(tips_string.begin(), tips_string.end(), std::back_inserter(tips),
                 [](const std::string& item) { return blk_hash_t(item); });
  return ok;
}

bool BlockProposer::getShardedTrxs(uint total_shard, uint my_shard, vec_trx_t& sharded_trxs) {
  vec_trx_t to_be_packed_trx;
  trx_mgr_->packTrxs(to_be_packed_trx, bp_config_.transaction_limit);

  if (to_be_packed_trx.empty()) {
    LOG(log_tr_) << "Skip block proposer, zero unpacked transactions ..." << std::endl;
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
    LOG(log_tr_) << "Skip block proposer, zero sharded transactions ..." << std::endl;
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

level_t BlockProposer::getProposeLevel(blk_hash_t const& pivot, vec_blk_t const& tips) {
  level_t max_level = 0;
  // get current level
  auto pivot_blk = dag_blk_mgr_->getDagBlock(pivot);
  if (!pivot_blk) {
    LOG(log_er_) << "Cannot find pivot dag block " << pivot;
    return 0;
  }
  max_level = std::max(pivot_blk->getLevel(), max_level);

  for (auto const& t : tips) {
    auto tip_blk = dag_blk_mgr_->getDagBlock(t);
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
  dag_blk_mgr_->insertBlock(blk);

  auto now = getCurrentTimeMilliSeconds();
  LOG(log_time_) << "Propose block " << blk.getHash() << " at: " << now << " ,trxs: " << blk.getTrxs()
                 << " , tips: " << blk.getTips().size();
  LOG(log_nf_) << " Propose block :" << blk.getHash() << " pivot: " << blk.getPivot() << " , number of trx ("
               << blk.getTrxs().size() << ")";
  BlockProposer::num_proposed_blocks.fetch_add(1);
}

bool BlockProposer::validDposProposer(level_t const propose_level) {
  uint64_t period = dag_blk_mgr_->getPeriod(propose_level);
  return final_chain_->dpos_is_eligible(period, node_addr_);
}

}  // namespace taraxa
