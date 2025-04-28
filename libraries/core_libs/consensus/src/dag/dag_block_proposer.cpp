#include "dag/dag_block_proposer.hpp"

#include "common/util.hpp"
#include "dag/dag_manager.hpp"
#include "final_chain/final_chain.hpp"
#include "key_manager/key_manager.hpp"
#include "transaction/transaction.hpp"
#include "transaction/transaction_manager.hpp"

namespace taraxa {

using namespace vdf_sortition;

DagBlockProposer::DagBlockProposer(const FullNodeConfig& config, std::shared_ptr<DagManager> dag_mgr,
                                   std::shared_ptr<TransactionManager> trx_mgr,
                                   std::shared_ptr<final_chain::FinalChain> final_chain, std::shared_ptr<DbStorage> db,
                                   std::shared_ptr<KeyManager> key_manager)
    : bp_config_(config.genesis.dag.block_proposer),
      total_trx_shards_(std::max(bp_config_.shard, uint16_t(1))),
      dag_mgr_(std::move(dag_mgr)),
      trx_mgr_(std::move(trx_mgr)),
      final_chain_(std::move(final_chain)),
      key_manager_(std::move(key_manager)),
      db_(std::move(db)),
      node_addr_(dev::toAddress(config.node_secret)),
      node_sk_(config.node_secret),
      vrf_sk_(config.vrf_secret),
      vrf_pk_(vrf_wrapper::getVrfPublicKey(vrf_sk_)),
      kDagProposeGasLimit(
          std::min(config.propose_dag_gas_limit, config.genesis.getGasLimits(final_chain_->lastBlockNumber()).first)),
      kPbftGasLimit(config.genesis.getGasLimits(final_chain_->lastBlockNumber()).second),
      kDagGasLimit(config.genesis.getGasLimits(final_chain_->lastBlockNumber()).first),
      kHardforks(config.genesis.state.hardforks),
      kValidatorMaxVote(config.genesis.state.dpos.validator_maximum_stake /
                        config.genesis.state.dpos.vote_eligibility_balance_step) {
  const auto& node_addr = node_addr_;
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

  // Do not propose dag blocks if number of non finalized transactions is over the limit
  if (trx_mgr_->getNonfinalizedTrxSize() > kMaxNonFinalizedTransactions) {
    return false;
  }

  auto frontier = dag_mgr_->getDagFrontier();
  LOG(log_dg_) << "Get frontier with pivot: " << frontier.pivot << " tips: " << frontier.tips;
  assert(!frontier.pivot.isZero());
  const auto propose_level = getProposeLevel(frontier.pivot, frontier.tips) + 1;

  const auto proposal_period = db_->getProposalPeriodForDagLevel(propose_level);
  if (!proposal_period.has_value()) {
    LOG(log_wr_) << "No proposal period for propose_level " << propose_level << " found";
    return false;
  }

  if (*proposal_period + kDagExpiryLevelLimit < final_chain_->lastBlockNumber()) {
    LOG(log_wr_) << "Trying to propose old block " << propose_level;
    return false;
  }

  if (!isValidDposProposer(*proposal_period)) {
    return false;
  }

  auto pk = key_manager_->getVrfKey(*proposal_period, node_addr_);
  if (pk && *pk != vrf_pk_) {
    LOG(log_er_) << "VRF public key mismatch " << *pk << " - " << vrf_pk_;
    return false;
  }

  uint64_t max_vote_count = 0;
  const auto vote_count = final_chain_->dposEligibleVoteCount(*proposal_period, node_addr_);
  if (*proposal_period < kHardforks.magnolia_hf.block_num) {
    max_vote_count = final_chain_->dposEligibleTotalVoteCount(*proposal_period);
  } else {
    max_vote_count = kValidatorMaxVote;
  }

  if (max_vote_count == 0) {
    LOG(log_er_) << "Total vote count 0 at proposal period:" << *proposal_period;
    return false;
  }

  const auto period_block_hash = db_->getPeriodBlockHash(*proposal_period);
  // get sortition
  const auto sortition_params = dag_mgr_->sortitionParamsManager().getSortitionParams(*proposal_period);
  vdf_sortition::VdfSortition vdf(sortition_params, vrf_sk_,
                                  VrfSortitionBase::makeVrfInput(propose_level, period_block_hash), vote_count,
                                  max_vote_count);

  auto anchor = dag_mgr_->getAnchors().second;
  if (frontier.pivot != anchor) {
    if (dag_mgr_->getNonFinalizedBlocksSize().second > kMaxNonFinalizedDagBlocks) {
      return false;
    }
    if (dag_mgr_->getNonFinalizedBlocksMinDifficulty() < vdf.getDifficulty()) {
      return false;
    }
  }

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

  auto [transactions, estimations] = getShardedTrxs(*proposal_period, kDagProposeGasLimit);
  if (transactions.empty()) {
    last_propose_level_ = propose_level;
    num_tries_ = 0;
    return false;
  }

  dev::bytes vdf_msg = DagManager::getVdfMessage(frontier.pivot, transactions);

  std::atomic_bool cancellation_token = false;
  std::promise<void> sync;
  executor_.post([&vdf, &sortition_params, &vdf_msg, cancel = std::ref(cancellation_token), &sync]() mutable {
    vdf.computeVdfSolution(sortition_params, vdf_msg, cancel);
    sync.set_value();
  });

  std::future<void> result = sync.get_future();
  while (result.wait_for(std::chrono::milliseconds(100)) != std::future_status::ready) {
    auto latest_frontier = dag_mgr_->getDagFrontier();
    const auto latest_level = getProposeLevel(latest_frontier.pivot, latest_frontier.tips) + 1;
    if (latest_level > propose_level && vdf.getDifficulty() > sortition_params.vdf.difficulty_min) {
      cancellation_token = true;
      break;
    }
  }

  if (cancellation_token) {
    last_propose_level_ = propose_level;
    num_tries_ = 0;
    result.wait();
    // Since compute was canceled there is a chance to propose a new block immediately, return true to skip sleep
    return true;
  }

  if (vdf.isStale(sortition_params)) {
    // Computing VDF for a stale block is CPU extensive, there is a possibility that some dag blocks are in a queue,
    // give it a second to process these dag blocks
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

  auto dag_block =
      createDagBlock(std::move(frontier), propose_level, transactions, std::move(estimations), std::move(vdf));

  if (dag_mgr_->addDagBlock(dag_block, std::move(transactions), true).first) {
    LOG(log_nf_) << "Proposed new DAG block " << dag_block->getHash() << ", pivot " << dag_block->getPivot()
                 << " , txs num " << dag_block->getTrxs().size();
    proposed_blocks_count_ += 1;
  } else {
    LOG(log_er_) << "Failed to add newly proposed dag block " << dag_block->getHash() << " into dag";
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
      auto packets_over_the_limit = false;
      if (auto net = network_.lock()) {
        syncing = net->pbft_syncing();
        packets_over_the_limit = net->packetQueueOverLimit();
      }
      // Only sleep if block was not proposed or if we are syncing or if packets queue is over the limit, if block is
      // proposed try to propose another block immediately
      if (syncing || packets_over_the_limit || !proposeDagBlock()) {
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

  if (total_trx_shards_ == 1) return trx_mgr_->packTrxs(proposal_period, weight_limit);

  auto [transactions, estimations] = trx_mgr_->packTrxs(proposal_period, weight_limit);

  if (transactions.empty()) {
    LOG(log_tr_) << "Skip block proposer, zero unpacked transactions ..." << std::endl;
    return {};
  }
  SharedTransactions sharded_trxs;
  std::vector<uint64_t> sharded_estimations;
  for (uint32_t i = 0; i < transactions.size(); i++) {
    auto shard = std::stoull(transactions[i]->getSender().toString().substr(0, 10), NULL, 16) +
                 proposal_period / kShardProposePeriodInterval;
    if (shard % total_trx_shards_ == my_trx_shard_) {
      sharded_trxs.emplace_back(transactions[i]);
      sharded_estimations.emplace_back(estimations[i]);
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

vec_blk_t DagBlockProposer::selectDagBlockTips(const vec_blk_t& frontier_tips, uint64_t gas_limit) const {
  // Prioritize higher level with unique proposer
  uint64_t gas_estimation = 0;
  std::unordered_set<addr_t> proposers, duplicate_proposers;
  std::unordered_map<blk_hash_t, std::shared_ptr<DagBlock>> tips_blocks;
  tips_blocks.reserve(frontier_tips.size());
  vec_blk_t tips;
  tips.reserve(kDagBlockMaxTips);

  // Retrieve all the tips blocks and identify duplicate proposer tips
  for (const auto& t : frontier_tips) {
    auto tip_block = dag_mgr_->getDagBlock(t);
    if (tip_block == nullptr) {
      // This could happen if a tip block has expired, exclude this tip
      LOG(log_nf_) << "selectDagBlockTips, Cannot find tip dag block " << tip_block;
    } else {
      tips_blocks.insert({t, tip_block});
      if (!proposers.insert(tip_block->getSender()).second) {
        duplicate_proposers.insert(tip_block->getSender());
      }
    }
  }

  // Split tips by tips with unique or duplicate proposers
  std::multimap<level_t, blk_hash_t, std::greater<level_t>> ordered_tips_with_unique_proposers,
      ordered_tips_with_duplicate_proposers;
  for (const auto& t : frontier_tips) {
    const auto& tip_block = tips_blocks[t];
    if (duplicate_proposers.contains(tip_block->getSender())) {
      ordered_tips_with_duplicate_proposers.insert({tip_block->getLevel(), t});
    } else {
      ordered_tips_with_unique_proposers.insert({tip_block->getLevel(), t});
    }
  }

  // First insert tips with unique proposers ordered by level
  for (const auto& tip : ordered_tips_with_unique_proposers) {
    gas_estimation += tips_blocks[tip.second]->getGasEstimation();
    if (gas_estimation > gas_limit || tips.size() == kDagBlockMaxTips) {
      return tips;
    }
    tips.push_back(tip.second);
  }

  // After inserting all unique proposers blocks, insert duplicates as well
  for (const auto& tip : ordered_tips_with_duplicate_proposers) {
    gas_estimation += tips_blocks[tip.second]->getGasEstimation();
    if (gas_estimation > gas_limit || tips.size() == kDagBlockMaxTips) {
      return tips;
    }
    tips.push_back(tip.second);
  }
  return tips;
}

std::shared_ptr<DagBlock> DagBlockProposer::createDagBlock(DagFrontier&& frontier, level_t level,
                                                           const SharedTransactions& trxs,
                                                           std::vector<uint64_t>&& estimations,
                                                           VdfSortition&& vdf) const {
  // When we propose block we know it is valid, no need for block verification with queue,
  // simply add the block to the DAG
  vec_trx_t trx_hashes;
  for (const auto& trx : trxs) {
    trx_hashes.push_back(trx->getHash());
  }

  const uint64_t block_estimation = std::accumulate(estimations.begin(), estimations.end(), 0);

  // If number of tips is over the limit filter by producer and level
  if (frontier.tips.size() > kDagBlockMaxTips || (frontier.tips.size() + 1) > kPbftGasLimit / kDagGasLimit) {
    frontier.tips = selectDagBlockTips(frontier.tips, kPbftGasLimit - block_estimation);
  }

  return std::make_shared<DagBlock>(frontier.pivot, std::move(level), std::move(frontier.tips), std::move(trx_hashes),
                                    block_estimation, std::move(vdf), node_sk_);
}

bool DagBlockProposer::isValidDposProposer(PbftPeriod propose_period) const {
  if (final_chain_->lastBlockNumber() < propose_period) {
    LOG(log_wr_) << "Last finalized block period " << final_chain_->lastBlockNumber() << " < propose_period "
                 << propose_period;
    return false;
  }

  try {
    return final_chain_->dposIsEligible(propose_period, node_addr_);
  } catch (state_api::ErrFutureBlock& c) {
    LOG(log_wr_) << "Proposal period " << propose_period << " is too far ahead of DPOS. " << c.what();
    return false;
  }
}

void DagBlockProposer::setNetwork(std::weak_ptr<Network> network) { network_ = std::move(network); }

}  // namespace taraxa
