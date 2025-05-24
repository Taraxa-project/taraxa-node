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
    : total_trx_shards_(std::max(config.genesis.dag.block_proposer.shard, uint16_t(1))),
      dag_mgr_(std::move(dag_mgr)),
      trx_mgr_(std::move(trx_mgr)),
      final_chain_(std::move(final_chain)),
      key_manager_(std::move(key_manager)),
      db_(std::move(db)),
      nodes_dag_proposers_data_(),
      kDagProposeGasLimit(
          std::min(config.propose_dag_gas_limit, config.genesis.getGasLimits(final_chain_->lastBlockNumber()).first)),
      kPbftGasLimit(config.genesis.getGasLimits(final_chain_->lastBlockNumber()).second),
      kDagGasLimit(config.genesis.getGasLimits(final_chain_->lastBlockNumber()).first),
      kHardforks(config.genesis.state.hardforks),
      kValidatorMaxVote(config.genesis.state.dpos.validator_maximum_stake /
                        config.genesis.state.dpos.vote_eligibility_balance_step),
      logger_(spdlogger::Logging::get().CreateChannelLogger("DAG_PROPOSER")) {
  for (const auto& wallet : config.wallets) {
    nodes_dag_proposers_data_.emplace_back(wallet, max_num_tries_, config.genesis.dag.block_proposer.shard);
  }
}

bool DagBlockProposer::proposeDagBlock() {
  if (trx_mgr_->getTransactionPoolSize() == 0) {
    return false;
  }

  // Do not propose dag blocks if number of non finalized transactions is over the limit
  if (trx_mgr_->getNonfinalizedTrxSize() > kMaxNonFinalizedTransactions) {
    return false;
  }

  // Try to propose dag block for each wallet
  bool block_proposed = false;
  for (auto& node_dag_proposer_data : nodes_dag_proposers_data_) {
    auto frontier = dag_mgr_->getDagFrontier();
    logger_->debug(
      "Get frontier with pivot: {}"
      " tips: {}",
      frontier.pivot, frontier.tips);
    assert(!frontier.pivot.isZero());
    const auto propose_level = getProposeLevel(frontier.pivot, frontier.tips) + 1;

    const auto proposal_period = db_->getProposalPeriodForDagLevel(propose_level);
    if (!proposal_period.has_value()) {
      logger_->warn(
        "No proposal period for propose_level {}"
        " found",
        propose_level);
      return false;
    }

    if (*proposal_period + kDagExpiryLevelLimit < final_chain_->lastBlockNumber()) {
      logger_->warn("Trying to propose old block {}", propose_level);
      return false;
    }

    if (!isValidDposProposer(*proposal_period, node_dag_proposer_data.wallet.node_addr)) {
      continue;
    }

    auto pk = key_manager_->getVrfKey(*proposal_period, node_dag_proposer_data.wallet.node_addr);
    if (pk && *pk != node_dag_proposer_data.wallet.vrf_pk) {
      logger_->error(
          "VRF public key mismatch {}"
          " - {}",
          *pk, node_dag_proposer_data.wallet.vrf_pk);
      continue;
    }

    uint64_t max_vote_count = 0;
    const auto vote_count =
        final_chain_->dposEligibleVoteCount(*proposal_period, node_dag_proposer_data.wallet.node_addr);
    if (*proposal_period < kHardforks.magnolia_hf.block_num) {
      max_vote_count = final_chain_->dposEligibleTotalVoteCount(*proposal_period);
    } else {
      max_vote_count = kValidatorMaxVote;
    }

    if (max_vote_count == 0) {
      logger_->error("{} total vote count 0 at proposal period: {}", node_dag_proposer_data.wallet.node_addr,
                     *proposal_period);
      continue;
    }

    const auto period_block_hash = db_->getPeriodBlockHash(*proposal_period);
    // get sortition
    const auto sortition_params = dag_mgr_->sortitionParamsManager().getSortitionParams(*proposal_period);
    vdf_sortition::VdfSortition vdf(sortition_params, node_dag_proposer_data.wallet.vrf_secret,
                                    VrfSortitionBase::makeVrfInput(propose_level, period_block_hash), vote_count,
                                    max_vote_count);

    auto anchor = dag_mgr_->getAnchors().second;
    if (frontier.pivot != anchor) {
      if (dag_mgr_->getNonFinalizedBlocksSize().second > kMaxNonFinalizedDagBlocks) {
        continue;
      }
      if (dag_mgr_->getNonFinalizedBlocksMinDifficulty() < vdf.getDifficulty() &&
          dag_mgr_->getNonFinalizedBlocksSize().second > kMaxNonFinalizedDagBlocksLowDifficulty) {
        continue;
      }
    }

    if (vdf.isStale(sortition_params)) {
      if (node_dag_proposer_data.last_propose_level == propose_level) {
        if (node_dag_proposer_data.num_tries < node_dag_proposer_data.max_num_tries) {
          logger_->debug("{} will not propose DAG block. Get difficulty at stale, tried {} times.",
                         node_dag_proposer_data.wallet.node_addr, node_dag_proposer_data.num_tries);
          node_dag_proposer_data.num_tries++;
          continue;
        }
      } else {
        logger_->debug(
            "{} will not propose DAG block, will reset number of tries. Get difficulty at stale , current propose "
            "level {}",
            node_dag_proposer_data.wallet.node_addr, propose_level);
        node_dag_proposer_data.last_propose_level = propose_level;
        node_dag_proposer_data.num_tries = 0;
        continue;
      }
    }

    auto [transactions, estimations] =
        getShardedTrxs(*proposal_period, kDagProposeGasLimit, node_dag_proposer_data.trx_shard);
    if (transactions.empty()) {
      node_dag_proposer_data.last_propose_level = propose_level;
      node_dag_proposer_data.num_tries = 0;
      continue;
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
      if (latest_level > propose_level + 1 && vdf.getDifficulty() > sortition_params.vdf.difficulty_min) {
        cancellation_token = true;
        break;
      }
    }

    if (cancellation_token) {
      node_dag_proposer_data.last_propose_level = propose_level;
      node_dag_proposer_data.num_tries = 0;
      result.wait();
      // Since compute was canceled there is a chance to propose a new block immediately, return true to skip sleep
      block_proposed = true;
      continue;
    }

    if (vdf.isStale(sortition_params)) {
      // Computing VDF for a stale block is CPU extensive, there is a possibility that some dag blocks are in a queue,
      // give it a second to process these dag blocks
      thisThreadSleepForSeconds(1);
      auto latest_frontier = dag_mgr_->getDagFrontier();
      const auto latest_level = getProposeLevel(latest_frontier.pivot, latest_frontier.tips) + 1;
      if (latest_level > propose_level) {
        node_dag_proposer_data.last_propose_level = propose_level;
        node_dag_proposer_data.num_tries = 0;
        continue;
      }
    }

    logger_->debug("{} VDF computation time {} difficulty {}", node_dag_proposer_data.wallet.node_addr,
                   vdf.getComputationTime(), vdf.getDifficulty());

    auto dag_block = createDagBlock(std::move(frontier), propose_level, transactions, std::move(estimations),
                                    std::move(vdf), node_dag_proposer_data.wallet.node_secret);

    if (dag_mgr_->addDagBlock(dag_block, std::move(transactions), true).first) {
      logger_->info("{} proposed new DAG block {}, pivot {}, txs num {}", node_dag_proposer_data.wallet.node_addr,
                    dag_block->getHash(), dag_block->getPivot(), dag_block->getTrxs().size());
      proposed_blocks_count_ += 1;
      block_proposed = true;
    } else {
      logger_->error("Failed to add newly proposed dag block {}, proposed by {} into dag", dag_block->getHash(),
                     node_dag_proposer_data.wallet.node_addr);
    }

    node_dag_proposer_data.last_propose_level = propose_level;
    node_dag_proposer_data.num_tries = 0;
  }

  return block_proposed;
}

void DagBlockProposer::start() {
  if (bool b = true; !stopped_.compare_exchange_strong(b, !b)) {
    return;
  }
  const uint16_t min_proposal_delay = 100;

  logger_->info("DagBlockProposer started ...");

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

  logger_->info("DagBlockProposer stopped ...");
}

std::pair<SharedTransactions, std::vector<uint64_t>> DagBlockProposer::getShardedTrxs(
    PbftPeriod proposal_period, uint64_t weight_limit, const uint16_t node_trx_shard) const {
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
    logger_->trace("Skip block proposer, zero unpacked transactions ...");
    return {};
  }
  SharedTransactions sharded_trxs;
  std::vector<uint64_t> sharded_estimations;
  for (uint32_t i = 0; i < transactions.size(); i++) {
    auto shard = std::stoull(transactions[i]->getSender().toString().substr(0, 10), NULL, 16) +
                 proposal_period / kShardProposePeriodInterval;
    if (shard % total_trx_shards_ == node_trx_shard) {
      sharded_trxs.emplace_back(transactions[i]);
      sharded_estimations.emplace_back(estimations[i]);
    }
  }
  if (sharded_trxs.empty()) {
    logger_->trace("Skip block proposer, zero sharded transactions ...");
    return {};
  }
  return {sharded_trxs, sharded_estimations};
}

level_t DagBlockProposer::getProposeLevel(blk_hash_t const& pivot, vec_blk_t const& tips) const {
  level_t max_level = 0;
  // get current level
  auto pivot_blk = dag_mgr_->getDagBlock(pivot);
  if (!pivot_blk) {
    logger_->error("Cannot find pivot dag block {}", pivot);
    return 0;
  }
  max_level = std::max(pivot_blk->getLevel(), max_level);

  for (auto const& t : tips) {
    auto tip_blk = dag_mgr_->getDagBlock(t);
    if (!tip_blk) {
      logger_->error("Cannot find tip dag block {}", t);
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
      logger_->info("selectDagBlockTips, Cannot find tip dag block {}", tip_block->toString());
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
                                                           std::vector<uint64_t>&& estimations, VdfSortition&& vdf,
                                                           const dev::Secret& node_secret) const {
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
                                    block_estimation, std::move(vdf), node_secret);
}

bool DagBlockProposer::isValidDposProposer(PbftPeriod propose_period, const addr_t& node_addr) const {
  if (final_chain_->lastBlockNumber() < propose_period) {
    logger_->warn("Last finalized block period {} < propose_period {}", final_chain_->lastBlockNumber(),
                  propose_period);
    return false;
  }

  try {
    return final_chain_->dposIsEligible(propose_period, node_addr);
  } catch (state_api::ErrFutureBlock& c) {
    logger_->warn("Proposal period {} is too far ahead of DPOS. {}", propose_period, c.what());
    return false;
  }
}

void DagBlockProposer::setNetwork(std::weak_ptr<Network> network) { network_ = std::move(network); }

}  // namespace taraxa
