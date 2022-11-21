#include "dag/dag_block_proposer.hpp"

#include <cmath>

#include "common/util.hpp"
#include "dag/dag_manager.hpp"
#include "final_chain/final_chain.hpp"
#include "key_manager/key_manager.hpp"
#include "transaction/transaction.hpp"
#include "transaction/transaction_manager.hpp"

namespace taraxa {

using namespace vdf_sortition;

DagBlockProposer::DagBlockProposer(const DagBlockProposerConfig& bp_config, std::shared_ptr<DagManager> dag_mgr,
                                   std::shared_ptr<TransactionManager> trx_mgr,
                                   std::shared_ptr<final_chain::FinalChain> final_chain, std::shared_ptr<DbStorage> db,
                                   std::shared_ptr<KeyManager> key_manager, addr_t node_addr, secret_t node_sk,
                                   vrf_wrapper::vrf_sk_t vrf_sk)
    : bp_config_(bp_config),
      total_trx_shards_(std::max(bp_config_.shard, uint16_t(1))),
      dag_mgr_(std::move(dag_mgr)),
      trx_mgr_(std::move(trx_mgr)),
      final_chain_(std::move(final_chain)),
      key_manager_(std::move(key_manager)),
      db_(std::move(db)),
      node_addr_(node_addr),
      node_sk_(node_sk),
      vrf_sk_(vrf_sk),
      vrf_pk_(vrf_wrapper::getVrfPublicKey(vrf_sk)) {
  LOG_OBJECTS_CREATE("DAG_PROPOSER");

  // Add a random component in proposing stale blocks so that not all nodes propose stale blocks at the same time
  // This will make stale block be proposed after waiting random interval between 2 and 20 seconds
  max_num_tries_ += (node_addr_[0] % (10 * max_num_tries_));

  auto addr = std::stoull(node_addr.toString().substr(0, 6).c_str(), NULL, 16);
  my_trx_shard_ = addr % bp_config_.shard;

  LOG(log_nf_) << "Dag block proposer in " << my_trx_shard_ << " shard ...";
}

bool DagBlockProposer::proposeDagBlock() {
  if (trx_mgr_->getTransactionPoolSize() == 0) {
    return false;
  }

  auto frontier = dag_mgr_->getDagFrontier();
  LOG(log_dg_) << "Get frontier with pivot: " << frontier.pivot << " tips: " << frontier.tips;
  assert(!frontier.pivot.isZero());
  auto propose_level = getProposeLevel(frontier.pivot, frontier.tips) + 1;

  const auto proposal_period = db_->getProposalPeriodForDagLevel(propose_level);
  if (!proposal_period.has_value()) {
    LOG(log_er_) << "No proposal period for propose_level " << propose_level << " found";
    assert(false);
  }

  if (!isValidDposProposer(*proposal_period)) {
    return false;
  }

  auto pk = key_manager_->get(*proposal_period, node_addr_);
  if (pk && *pk != vrf_pk_) {
    LOG(log_er_) << "VRF public key mismatch " << *pk << " - " << vrf_pk_;
    return false;
  }

  const auto period_block_hash = db_->getPeriodBlockHash(*proposal_period);
  // get sortition
  const auto sortition_params = dag_mgr_->sortitionParamsManager().getSortitionParams(*proposal_period);
  vdf_sortition::VdfSortition vdf(sortition_params, vrf_sk_,
                                  VrfSortitionBase::makeVrfInput(propose_level, period_block_hash));
  if (vdf.isStale(sortition_params)) {
    if (last_propose_level_ == propose_level) {
      if (num_tries_ < max_num_tries_) {
        LOG(log_dg_) << "Will not propose DAG block. Get difficulty at stale, tried " << num_tries_ << " times.";
        num_tries_++;
        return false;
      }
    } else {
      LOG(log_dg_) << "Will not propose DAG block, will reset number of tries. Get difficulty at stale , current "
                      "propose level "
                   << propose_level;
      last_propose_level_ = propose_level;
      num_tries_ = 0;
      return false;
    }
  }

  std::atomic_bool cancellation_token = false;
  auto vdf_result = std::async(std::launch::async, &VdfSortition::computeVdfSolution, &vdf, std::ref(sortition_params),
                               frontier.pivot.asBytes(), std::ref(cancellation_token));
  while (vdf_result.wait_for(std::chrono::milliseconds(100)) != std::future_status::ready) {
    auto latest_frontier = dag_mgr_->getDagFrontier();
    const auto latest_level = getProposeLevel(latest_frontier.pivot, latest_frontier.tips) + 1;

    // Stop computation in case there is already higher level in dag than level we are currently computing vdf for
    if (latest_level > propose_level) {
      cancellation_token = true;
      last_propose_level_ = propose_level;
      num_tries_ = 0;
      vdf_result.wait();
      // Since compute was canceled there is a chance to propose a new block immediately, return true to skip sleep
      return true;
    }
  }

  if (vdf.isStale(sortition_params)) {
    // Computing VDF for a stale block is CPU extensive, there is a possibility that some dag blocks are in a queue,
    // give it a second to process these dag blocks
    // TODO: remove thisThreadSleepForSeconds
    thisThreadSleepForSeconds(1);
    auto latest_frontier = dag_mgr_->getDagFrontier();
    const auto latest_level = getProposeLevel(latest_frontier.pivot, latest_frontier.tips) + 1;
    if (latest_level > propose_level) {
      last_propose_level_ = propose_level;
      num_tries_ = 0;
      return false;
    }
  }
  LOG(log_dg_) << "VDF computation time " << vdf.getComputationTime() << " difficulty " << vdf.getDifficulty();

  auto [transactions, estimations] = getShardedTrxs(*proposal_period, dag_mgr_->getDagConfig().gas_limit);
  if (transactions.empty()) {
    last_propose_level_ = propose_level;
    num_tries_ = 0;
    return false;
  }

  auto dag_block =
      createDagBlock(std::move(frontier), propose_level, transactions, std::move(estimations), std::move(vdf));

  if (dag_mgr_->addDagBlock(std::move(dag_block), std::move(transactions), true).first) {
    LOG(log_er_) << "Failed to add newly proposed dag block into dag";
    proposed_blocks_count_ += 1;
  } else {
    LOG(log_nf_) << "Proposed new DAG block " << dag_block.getHash() << ", pivot " << dag_block.getPivot()
                 << " , txs num " << dag_block.getTrxs().size();
  }

  last_propose_level_ = propose_level;
  num_tries_ = 0;

  return true;
}

void DagBlockProposer::start() {
  if (bool b = true; !stopped_.compare_exchange_strong(b, !b)) {
    return;
  }
  const uint16_t min_proposal_delay = 100;

  LOG(log_nf_) << "DagBlockProposer started ...";

  // reset number of proposed blocks
  proposed_blocks_count_ = 0;

  proposer_worker_ = std::make_shared<std::thread>([this]() {
    while (!stopped_) {
      // Blocks are not proposed if we are behind the network and still syncing
      auto syncing = false;
      if (auto net = network_.lock()) {
        syncing = net->pbft_syncing();
      }
      // Only sleep if block was not proposed or if we are syncing, if block is proposed try to propose another block
      // immediately
      if (syncing || !proposeDagBlock()) {
        thisThreadSleepForMilliSeconds(min_proposal_delay);
      }
    }
  });
}

void DagBlockProposer::stop() {
  if (bool b = false; !stopped_.compare_exchange_strong(b, !b)) {
    return;
  }
  proposer_worker_->join();

  LOG(log_nf_) << "DagBlockProposer stopped ...";
}

std::pair<SharedTransactions, std::vector<uint64_t>> DagBlockProposer::getShardedTrxs(PbftPeriod proposal_period,
                                                                                      uint64_t weight_limit) const {
  // If syncing return empty list
  auto syncing = false;
  if (auto net = network_.lock()) {
    syncing = net->pbft_syncing();
  }
  if (syncing) {
    return {};
  }

  if (total_trx_shards_ == 1) {
    return trx_mgr_->packTrxs(proposal_period, weight_limit);
  }

  auto [transactions, estimations] = trx_mgr_->packTrxs(proposal_period, weight_limit);
  if (transactions.empty()) {
    LOG(log_tr_) << "Skip block proposer, zero unpacked transactions ..." << std::endl;
    return {};
  }

  SharedTransactions sharded_trxs;
  std::vector<uint64_t> sharded_estimations;
  for (uint32_t i = 0; i < transactions.size(); i++) {
    auto shard = std::stoull(transactions[i]->getHash().toString().substr(0, 10), NULL, 16);
    if (shard % total_trx_shards_ == my_trx_shard_) {
      sharded_trxs.emplace_back(transactions[i]);
      estimations.emplace_back(estimations[i]);
    }
  }

  if (sharded_trxs.empty()) {
    LOG(log_tr_) << "Skip block proposer, zero sharded transactions ..." << std::endl;
    return {};
  }

  return {sharded_trxs, sharded_estimations};
}

level_t DagBlockProposer::getProposeLevel(blk_hash_t const& pivot, vec_blk_t const& tips) const {
  level_t max_level = 0;
  // get current level
  auto pivot_blk = dag_mgr_->getDagBlock(pivot);
  if (!pivot_blk) {
    LOG(log_er_) << "Cannot find pivot dag block " << pivot;
    return 0;
  }
  max_level = std::max(pivot_blk->getLevel(), max_level);

  for (auto const& t : tips) {
    auto tip_blk = dag_mgr_->getDagBlock(t);
    if (!tip_blk) {
      LOG(log_er_) << "Cannot find tip dag block " << t;
      return 0;
    }
    max_level = std::max(tip_blk->getLevel(), max_level);
  }
  return max_level;
}

DagBlock DagBlockProposer::createDagBlock(DagFrontier&& frontier, level_t level, const SharedTransactions& trxs,
                                          std::vector<uint64_t>&& estimations, VdfSortition&& vdf) const {
  // When we propose block we know it is valid, no need for block verification with queue,
  // simply add the block to the DAG
  vec_trx_t trx_hashes;
  for (const auto& trx : trxs) {
    trx_hashes.push_back(trx->getHash());
  }

  uint64_t block_estimation = 0;
  for (const auto& e : estimations) {
    block_estimation += e;
  }

  DagBlock block(frontier.pivot, std::move(level), std::move(frontier.tips), std::move(trx_hashes), block_estimation,
                 std::move(vdf), node_sk_);

  return block;
}

bool DagBlockProposer::isValidDposProposer(PbftPeriod propose_period) const {
  if (final_chain_->last_block_number() < propose_period) {
    LOG(log_dg_) << "Last finalized block period " << final_chain_->last_block_number() << " < propose_period "
                 << propose_period;
    return false;
  }

  try {
    return final_chain_->dpos_is_eligible(propose_period, node_addr_);
  } catch (state_api::ErrFutureBlock& c) {
    LOG(log_dg_) << "Proposal period " << propose_period << " is too far ahead of DPOS. " << c.what();
    return false;
  }
}

void DagBlockProposer::setNetwork(std::weak_ptr<Network> network) { network_ = move(network); }

}  // namespace taraxa
