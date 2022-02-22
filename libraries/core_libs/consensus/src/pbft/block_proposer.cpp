#include "pbft/block_proposer.hpp"

#include <cmath>

#include "common/util.hpp"
#include "dag/dag.hpp"
#include "transaction/transaction.hpp"
#include "transaction_manager/transaction_manager.hpp"

namespace taraxa {

using namespace vdf_sortition;
std::atomic<uint64_t> BlockProposer::num_proposed_blocks = 0;

bool SortitionPropose::propose() {
  auto proposer = proposer_.lock();
  if (!proposer) {
    LOG(log_er_) << "Block proposer not available";
    return false;
  }

  if (trx_mgr_->getTransactionPoolSize() == 0) {
    return false;
  }

  auto frontier = dag_mgr_->getDagFrontier();
  LOG(log_dg_) << "Get frontier with pivot: " << frontier.pivot << " tips: " << frontier.tips;
  assert(!frontier.pivot.isZero());
  auto propose_level = proposer->getProposeLevel(frontier.pivot, frontier.tips) + 1;
  if (!proposer->validDposProposer(propose_level)) {
    return false;
  }

  const auto proposal_period = dag_blk_mgr_->getProposalPeriod(propose_level);
  const auto period_block_hash = db_->getPeriodBlockHash(proposal_period.first);
  // get sortition
  const auto sortition_params = dag_blk_mgr_->sortitionParamsManager().getSortitionParams(proposal_period.first);
  vdf_sortition::VdfSortition vdf(sortition_params, vrf_sk_, makeVrfInput(propose_level, period_block_hash));
  if (vdf.isStale(sortition_params)) {
    if (last_frontier_.isEqual(frontier)) {
      if (num_tries_ < max_num_tries_) {
        LOG(log_dg_) << "Will not propose DAG block. Get difficulty at stale, tried " << num_tries_ << " times.";
        num_tries_++;
        return false;
      }
    } else {
      LOG(log_dg_)
          << "Will not propose DAG block, will reset number of tries. Get difficulty at stale , current propose level "
          << propose_level;
      last_frontier_ = frontier;
      num_tries_ = 0;
      return false;
    }
  }

  vdf.computeVdfSolution(sortition_params, frontier.pivot.asBytes());
  if (vdf.isStale(sortition_params)) {
    // Computing VDF for a stale block is CPU extensive, there is a possibility that some dag blocks are in a queue,
    // give it a second to process these dag blocks
    thisThreadSleepForSeconds(1);
    auto latest_frontier = dag_mgr_->getDagFrontier();
    if (!latest_frontier.isEqual(frontier)) {
      last_frontier_ = frontier;
      num_tries_ = 0;
      return false;
    }
  }

  SharedTransactions shared_trxs = proposer->getShardedTrxs();
  if (shared_trxs.empty()) {
    last_frontier_ = frontier;
    num_tries_ = 0;
    return false;
  }
  LOG(log_nf_) << "VDF computation time " << vdf.getComputationTime() << " difficulty " << vdf.getDifficulty();
  last_frontier_ = frontier;
  proposer->proposeBlock(std::move(frontier), propose_level, std::move(shared_trxs), std::move(vdf));
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
      auto syncing = false;
      if (auto net = network_.lock()) {
        syncing = net->pbft_syncing();
      }
      if (!syncing) {
        propose_model_->propose();
      }
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

SharedTransactions BlockProposer::getShardedTrxs() {
  // If syncing return empty list
  auto syncing = false;
  if (auto net = network_.lock()) {
    syncing = net->pbft_syncing();
  }
  if (syncing) {
    return {};
  }

  SharedTransactions to_be_packed_trx = trx_mgr_->packTrxs(bp_config_.transaction_limit);

  if (to_be_packed_trx.empty()) {
    LOG(log_tr_) << "Skip block proposer, zero unpacked transactions ..." << std::endl;
    return {};
  }
  SharedTransactions sharded_trxs;
  for (auto const& t : to_be_packed_trx) {
    auto shard = std::stoull(t->getHash().toString().substr(0, 10), NULL, 16);
    if (shard % total_trx_shards_ == my_trx_shard_) {
      sharded_trxs.emplace_back(t);
    }
  }
  if (sharded_trxs.empty()) {
    LOG(log_tr_) << "Skip block proposer, zero sharded transactions ..." << std::endl;
    return {};
  }
  return sharded_trxs;
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
      LOG(log_er_) << "Cannot find tip dag block " << t;
      return 0;
    }
    max_level = std::max(tip_blk->getLevel(), max_level);
  }
  return max_level;
}

void BlockProposer::proposeBlock(DagFrontier&& frontier, level_t level, SharedTransactions&& trxs, VdfSortition&& vdf) {
  if (stopped_) return;

  vec_trx_t trx_hashes;
  std::transform(trxs.begin(), trxs.end(), std::back_inserter(trx_hashes),
                 [](std::shared_ptr<Transaction> const& t) { return t->getHash(); });

  // When we propose block we know it is valid, no need for block verification with queue,
  // simply add the block to the DAG
  DagBlock blk(frontier.pivot, std::move(level), std::move(frontier.tips), std::move(trx_hashes), std::move(vdf),
               node_sk_);
  dag_mgr_->addDagBlock(blk, std::move(trxs), true);
  dag_blk_mgr_->markDagBlockAsSeen(blk);

  auto now = getCurrentTimeMilliSeconds();
  LOG(log_time_) << "Propose block " << blk.getHash() << " at: " << now << " ,trxs: " << blk.getTrxs()
                 << " , tips: " << blk.getTips().size();
  LOG(log_nf_) << "Add proposed DAG block " << blk.getHash() << ", pivot " << blk.getPivot() << " , number of trx ("
               << blk.getTrxs().size() << ")";
  BlockProposer::num_proposed_blocks.fetch_add(1);
}

bool BlockProposer::validDposProposer(level_t const propose_level) {
  auto proposal_period = dag_blk_mgr_->getProposalPeriod(propose_level);
  if (!proposal_period.second) {
    LOG(log_nf_) << "Cannot find the proposal level " << propose_level
                 << " in DB, too far ahead of proposal DAG blocks level";
    return false;
  }

  try {
    return final_chain_->dpos_is_eligible(proposal_period.first, node_addr_);
  } catch (state_api::ErrFutureBlock& c) {
    LOG(log_er_) << "Proposal period " << proposal_period.first << " is too far ahead of DPOS. " << c.what();
    return false;
  }
}

}  // namespace taraxa
