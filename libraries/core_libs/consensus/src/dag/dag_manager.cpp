#include "dag/dag_manager.hpp"

#include <libdevcore/CommonIO.h>

#include <algorithm>
#include <tuple>
#include <unordered_set>
#include <utility>
#include <vector>

#include "common/logger_formatters.hpp"
#include "config/config.hpp"
#include "dag/dag.hpp"
#include "key_manager/key_manager.hpp"
#include "network/network.hpp"
#include "transaction/transaction_manager.hpp"

namespace taraxa {

DagManager::DagManager(const FullNodeConfig &config, std::shared_ptr<TransactionManager> trx_mgr,
                       std::shared_ptr<PbftChain> pbft_chain, std::shared_ptr<final_chain::FinalChain> final_chain,
                       std::shared_ptr<DbStorage> db, std::shared_ptr<KeyManager> key_manager) try
    : max_level_(db->getLastBlocksLevel()),
      pivot_tree_(std::make_shared<PivotTree>(config.genesis.dag_genesis_block.getHash())),
      total_dag_(std::make_shared<Dag>(config.genesis.dag_genesis_block.getHash())),
      trx_mgr_(std::move(trx_mgr)),
      pbft_chain_(std::move(pbft_chain)),
      db_(std::move(db)),
      key_manager_(std::move(key_manager)),
      anchor_(config.genesis.dag_genesis_block.getHash()),
      period_(0),
      sortition_params_manager_(config.genesis.sortition, db_),
      dag_config_(config.genesis.dag),
      genesis_block_(std::make_shared<DagBlock>(config.genesis.dag_genesis_block)),
      max_levels_per_period_(config.max_levels_per_period),
      dag_expiry_limit_(config.dag_expiry_limit),
      seen_blocks_(cache_max_size_, cache_delete_step_),
      final_chain_(std::move(final_chain)),
      kGenesis(config.genesis),
      kValidatorMaxVote(config.genesis.state.dpos.validator_maximum_stake /
                        config.genesis.state.dpos.vote_eligibility_balance_step),
      logger_(logger::Logging::get().CreateChannelLogger("DAGMGR")) {
  if (auto ret = getLatestPivotAndTips(); ret) {
    frontier_.pivot = ret->first;
    for (const auto &t : ret->second) {
      frontier_.tips.push_back(t);
    }
  }
  // Set DAG level proposal period map
  if (!db_->getProposalPeriodForDagLevel(max_levels_per_period_)) {
    // Node start from scratch
    db_->saveProposalPeriodDagLevelsMap(max_levels_per_period_, 0);
  }
  recoverDag();
} catch (std::exception &e) {
  std::cerr << e.what() << std::endl;
}

std::shared_ptr<DagManager> DagManager::getShared() {
  try {
    return shared_from_this();
  } catch (std::bad_weak_ptr &e) {
    std::cerr << "DagManager: " << e.what() << std::endl;
    return nullptr;
  }
}

std::pair<uint64_t, uint64_t> DagManager::getNumVerticesInDag() const {
  std::shared_lock lock(mutex_);
  return {db_->getDagBlocksCount(), total_dag_->getNumVertices()};
}

std::pair<uint64_t, uint64_t> DagManager::getNumEdgesInDag() const {
  std::shared_lock lock(mutex_);
  return {db_->getDagEdgeCount(), total_dag_->getNumEdges()};
}

void DagManager::drawTotalGraph(std::string const &str) const {
  std::shared_lock lock(mutex_);
  total_dag_->drawGraph(str);
}

void DagManager::drawPivotGraph(std::string const &str) const {
  std::shared_lock lock(mutex_);
  pivot_tree_->drawGraph(str);
}

std::pair<bool, std::vector<blk_hash_t>> DagManager::pivotAndTipsAvailable(const std::shared_ptr<DagBlock> &blk) {
  auto dag_blk_hash = blk->getHash();
  const auto pivot_hash = blk->getPivot();
  const auto dag_blk_pivot = getDagBlock(pivot_hash);
  std::vector<blk_hash_t> missing_tips_or_pivot;

  level_t expected_level = 0;
  if (dag_blk_pivot) {
    expected_level = dag_blk_pivot->getLevel() + 1;
  } else {
    logger_->info(
        "DAG Block {}"
        " pivot {}"
        " unavailable",
        dag_blk_hash.toString(), pivot_hash.toString());
    missing_tips_or_pivot.push_back(pivot_hash);
  }

  for (auto const &tip : blk->getTips()) {
    auto tip_block = getDagBlock(tip);
    if (tip_block) {
      expected_level = std::max(expected_level, tip_block->getLevel() + 1);
    } else {
      logger_->info(
          "DAG Block {}"
          " tip {}"
          " unavailable",
          dag_blk_hash, tip);
      missing_tips_or_pivot.push_back(tip);
    }
  }

  if (missing_tips_or_pivot.size() > 0) {
    return {false, missing_tips_or_pivot};
  }

  if (expected_level != blk->getLevel()) {
    logger_->error(
        "DAG Block {}"
        " level {}"
        ", expected level: {}",
        dag_blk_hash, blk->getLevel(), expected_level);
    return {false, missing_tips_or_pivot};
  }

  return {true, missing_tips_or_pivot};
}

DagFrontier DagManager::getDagFrontier() {
  std::shared_lock lock(mutex_);
  return frontier_;
}

std::pair<bool, std::vector<blk_hash_t>> DagManager::addDagBlock(const std::shared_ptr<DagBlock> &blk,
                                                                 SharedTransactions &&trxs, bool proposed, bool save) {
  auto blk_hash = blk->getHash();

  {
    // One mutex protects the DagManager internal state, the other mutex ensures that dag blocks are gossiped in
    // correct order since multiple threads can call this method. There is a need for using two mutexes since having
    // blocks gossip under mutex_ leads to a deadlock with mutex in TaraxaPeer
    std::scoped_lock order_lock(order_dag_blocks_mutex_);
    {
      std::scoped_lock lock(mutex_);
      if (save) {
        if (db_->dagBlockInDb(blk->getHash())) {
          // It is a valid scenario that two threads can receive same block from two peers and process at same time
          return {true, {}};
        }

        if (blk->getLevel() < dag_expiry_level_) {
          logger_->info(
              "Dropping old block: {}"
              ". Expiry level: {}. Block level: {}",
              blk_hash, dag_expiry_level_, blk->getLevel());
          return {false, {}};
        }

        auto res = pivotAndTipsAvailable(blk);
        if (!res.first) {
          return res;
        }
        // Saves transactions and remove them from memory pool
        trx_mgr_->saveTransactionsFromDagBlock(trxs);
        // Save the dag block
        db_->saveDagBlock(blk);
      }
      seen_blocks_.insert(blk->getHash(), blk);
      auto pivot_hash = blk->getPivot();

      std::vector<blk_hash_t> tips = blk->getTips();
      level_t current_max_level = max_level_;
      max_level_ = std::max(current_max_level, blk->getLevel());

      addToDag(blk_hash, pivot_hash, tips, blk->getLevel());
      if (non_finalized_blks_min_difficulty_ > blk->getDifficulty()) {
        non_finalized_blks_min_difficulty_ = blk->getDifficulty();
      }

      updateFrontier();
    }
    if (save) {
      block_verified_.emit(blk);
      if (std::shared_ptr<Network> net = network_.lock()) {
        net->gossipDagBlock(blk, proposed, trxs);
      }
    }
  }
  logger_->info(
      "Update frontier after adding block {} anchor {}"
      " pivot = {}"
      " tips: {}",
      blk_hash, anchor_, frontier_.pivot, frontier_.tips);
  return {true, {}};
}

void DagManager::drawGraph(std::string const &dotfile) const {
  std::shared_lock lock(mutex_);
  drawPivotGraph("pivot." + dotfile);
  drawTotalGraph("total." + dotfile);
}

void DagManager::addToDag(blk_hash_t const &hash, blk_hash_t const &pivot, std::vector<blk_hash_t> const &tips,
                          uint64_t level, bool finalized) {
  total_dag_->addVEEs(hash, pivot, tips);
  pivot_tree_->addVEEs(hash, pivot, {});

  logger_->debug(" Insert block to DAG : {}", hash);
  if (finalized) {
    return;
  }

  if (!non_finalized_blks_[level].insert(hash).second) {
    logger_->error("Trying to insert duplicate block into the dag: {}", hash);
  }
}

std::optional<std::pair<blk_hash_t, std::vector<blk_hash_t>>> DagManager::getLatestPivotAndTips() const {
  std::shared_lock lock(mutex_);
  return {getFrontier()};
}

std::pair<blk_hash_t, std::vector<blk_hash_t>> DagManager::getFrontier() const {
  blk_hash_t pivot;
  std::vector<blk_hash_t> tips;

  auto last_pivot = anchor_;
  auto pivot_chain = pivot_tree_->getGhostPath(last_pivot);
  if (!pivot_chain.empty()) {
    pivot = pivot_chain.back();
    total_dag_->getLeaves(tips);
    // remove pivot from tips
    auto end = std::remove_if(tips.begin(), tips.end(), [pivot](blk_hash_t const &s) { return s == pivot; });
    tips.erase(end, tips.end());
  }
  return {pivot, tips};
}

void DagManager::updateFrontier() {
  auto [p, ts] = getFrontier();
  frontier_.pivot = p;
  frontier_.tips.clear();
  for (auto const &t : ts) {
    frontier_.tips.push_back(t);
  }
}

std::vector<blk_hash_t> DagManager::getGhostPath(const blk_hash_t &source) const {
  // No need to check ghost path for kNullBlockHash
  if (source == kNullBlockHash) {
    return {};
  }

  std::shared_lock lock(mutex_);
  return pivot_tree_->getGhostPath(source);
}

std::vector<blk_hash_t> DagManager::getGhostPath() const {
  std::shared_lock lock(mutex_);
  auto last_pivot = anchor_;
  return pivot_tree_->getGhostPath(last_pivot);
}

// return {block order}, for pbft-pivot-blk proposing
std::vector<blk_hash_t> DagManager::getDagBlockOrder(blk_hash_t const &anchor, PbftPeriod period) {
  std::shared_lock lock(mutex_);
  std::vector<blk_hash_t> blk_orders;

  if (period != period_ + 1) {
    logger_->warn(
        "getDagBlockOrder called with period {}"
        ". Expected period {}",
        period, period_ + 1);
    return {};
  }

  if (anchor_ == anchor) {
    logger_->warn(
        "Query period from {}"
        " to {}"
        " not ok",
        anchor_, anchor);
    return {};
  }

  auto new_period = period_ + 1;

  auto ok = total_dag_->computeOrder(anchor, blk_orders, non_finalized_blks_);
  if (!ok) {
    logger_->error(
        "Create period {}"
        " anchor: {}"
        " failed",
        new_period, anchor);
    return {};
  }

  logger_->debug(
      "Get period {}"
      " from {}"
      " to {}"
      " with {} blks",
      new_period, anchor_, anchor, blk_orders.size());

  return blk_orders;
}

uint DagManager::setDagBlockOrder(blk_hash_t const &new_anchor, PbftPeriod period, vec_blk_t const &dag_order) {
  logger_->debug(
      "setDagBlockOrder called with anchor {}"
      " and period {}",
      new_anchor, period);
  if (period != period_ + 1) {
    logger_->error(
        " Inserting period ({}"
        ") anchor {} does not match ..., previous internal period ({})",
        period, new_anchor, period_);
    return 0;
  }

  if (new_anchor == kNullBlockHash) {
    period_ = period;
    logger_->info(
        "Set new period {}"
        " with kNullBlockHash anchor",
        period);
    return 0;
  }

  // Update dag counts correctly
  // When synced and gossiping there should not be anything to update
  // When syncing we must check if some of the DAG blocks are both in period data and in memory DAG although
  // non-finalized block should be empty when syncing, maybe we should clear it if we are deep out of sync to improve
  // performance
  std::unordered_set<blk_hash_t> non_finalized_blocks_set;
  for (auto const &level : non_finalized_blks_) {
    for (auto const &blk : level.second) {
      non_finalized_blocks_set.insert(blk);
    }
  }
  // Only update counter for blocks that are in the dag_order and not in memory DAG, this is only possible when pbft
  // syncing and processing period data
  std::vector<std::shared_ptr<DagBlock>> dag_blocks_to_update_counters;
  for (auto const &blk : dag_order) {
    if (non_finalized_blocks_set.count(blk) == 0) {
      auto dag_block = getDagBlock(blk);
      dag_blocks_to_update_counters.push_back(dag_block);
    }
  }

  if (dag_blocks_to_update_counters.size()) {
    db_->updateDagBlockCounters(std::move(dag_blocks_to_update_counters));
  }

  total_dag_->clear();
  pivot_tree_->clear();
  auto non_finalized_blocks = std::move(non_finalized_blks_);
  non_finalized_blks_.clear();

  std::unordered_set<blk_hash_t> dag_order_set(dag_order.begin(), dag_order.end());
  assert(dag_order_set.count(new_anchor));
  addToDag(new_anchor, kNullBlockHash, vec_blk_t(), 0, true);

  const auto anchor_block_level = getDagBlock(new_anchor)->getLevel();
  if (anchor_block_level > dag_expiry_limit_) {
    dag_expiry_level_ = anchor_block_level - dag_expiry_limit_;
  }

  std::unordered_map<blk_hash_t, std::shared_ptr<DagBlock>> expired_dag_blocks_to_remove;
  std::vector<trx_hash_t> expired_dag_blocks_transactions;

  non_finalized_blks_min_difficulty_ = UINT32_MAX;
  for (auto &v : non_finalized_blocks) {
    for (auto &blk_hash : v.second) {
      if (dag_order_set.count(blk_hash) != 0) {
        continue;
      }

      auto dag_block = getDagBlock(blk_hash);
      auto pivot_hash = dag_block->getPivot();

      if (validateBlockNotExpired(dag_block, expired_dag_blocks_to_remove)) {
        addToDag(blk_hash, pivot_hash, dag_block->getTips(), dag_block->getLevel(), false);
        if (non_finalized_blks_min_difficulty_ > dag_block->getDifficulty()) {
          non_finalized_blks_min_difficulty_ = dag_block->getDifficulty();
        }
      } else {
        db_->removeDagBlock(blk_hash);
        seen_blocks_.erase(blk_hash);
        const auto dag_trxs = dag_block->getTrxs();
        std::copy(dag_trxs.begin(), dag_trxs.end(), std::back_inserter(expired_dag_blocks_transactions));
      }
    }
  }

  // Remove any transactions from expired dag blocks if not already finalized or included in another dag block
  if (expired_dag_blocks_transactions.size()) {
    handleExpiredDagBlocksTransactions(expired_dag_blocks_transactions);
  }

  old_anchor_ = anchor_;
  anchor_ = new_anchor;
  period_ = period;

  logger_->info(
      "Set new period {}"
      " with anchor {}",
      period, new_anchor);

  return dag_order_set.size();
}

void DagManager::handleExpiredDagBlocksTransactions(
    const std::vector<trx_hash_t> &transactions_from_expired_dag_blocks) const {
  std::unordered_set<trx_hash_t> transactions_from_expired_dag_blocks_to_remove;
  // Only transactions that need to be moved to memory pool are the ones that are not yet finalized and are not part of
  // another valid DAG block
  auto trxs_finalized = db_->transactionsFinalized(transactions_from_expired_dag_blocks);
  for (uint32_t i = 0; i < trxs_finalized.size(); i++) {
    if (!trxs_finalized[i]) {
      transactions_from_expired_dag_blocks_to_remove.emplace(transactions_from_expired_dag_blocks[i]);
    }
  }
  for (auto const &level : non_finalized_blks_) {
    for (auto const &blk : level.second) {
      auto dag_block = getDagBlock(blk);
      for (auto const &trx : dag_block->getTrxs()) {
        transactions_from_expired_dag_blocks_to_remove.erase(trx);
      }
    }
  }
  if (transactions_from_expired_dag_blocks_to_remove.size() > 0) {
    trx_mgr_->removeNonFinalizedTransactions(std::move(transactions_from_expired_dag_blocks_to_remove));
  }
}

bool DagManager::validateBlockNotExpired(
    const std::shared_ptr<DagBlock> &dag_block,
    std::unordered_map<blk_hash_t, std::shared_ptr<DagBlock>> &expired_dag_blocks_to_remove) {
  // Check for expired dag blocks, in practice this should happen very rarely if some node is cut off from the rest of
  // the network. In normal cases DAG blocks will be finalized or an old dag block will not enter the DAG at all
  const auto &blk_hash = dag_block->getHash();
  bool block_points_to_expired_block = expired_dag_blocks_to_remove.contains(dag_block->getPivot());
  if (!block_points_to_expired_block) {
    for (const auto &tip : dag_block->getTips()) {
      if (expired_dag_blocks_to_remove.contains(tip)) {
        block_points_to_expired_block = true;
        break;
      }
    }
  }

  if (block_points_to_expired_block || dag_block->getLevel() < dag_expiry_level_) {
    logger_->info(
        "Dropping expired block in setDagBlockOrder: {}"
        ". Expiry level: {}. Block level: {}",
        blk_hash, dag_expiry_level_, dag_block->getLevel());
    expired_dag_blocks_to_remove[blk_hash] = dag_block;
    return false;
  }
  return true;
}

void DagManager::recoverDag() {
  if (pbft_chain_) {
    auto pbft_block_hash = pbft_chain_->getLastPbftBlockHash();
    if (pbft_block_hash) {
      period_ = pbft_chain_->getPbftBlockInChain(pbft_block_hash).getPeriod();
    }

    while (pbft_block_hash) {
      auto pbft_block = pbft_chain_->getPbftBlockInChain(pbft_block_hash);
      auto anchor = pbft_block.getPivotDagBlockHash();
      pbft_block_hash = pbft_block.getPrevBlockHash();
      if (anchor) {
        anchor_ = anchor;
        logger_->info("Recover anchor {}", anchor_);
        addToDag(anchor_, kNullBlockHash, vec_blk_t(), 0, true);

        const auto anchor_block_level = getDagBlock(anchor_)->getLevel();
        if (anchor_block_level > dag_expiry_limit_) {
          dag_expiry_level_ = anchor_block_level - dag_expiry_limit_;
        }
        break;
      }
    }

    if (pbft_block_hash) {
      auto pbft_block = pbft_chain_->getPbftBlockInChain(pbft_block_hash);
      auto anchor = pbft_block.getPivotDagBlockHash();
      if (anchor) {
        old_anchor_ = anchor;
        logger_->info("Recover old anchor {}", old_anchor_);
      }
    }
  }

  for (auto &lvl : db_->getNonfinalizedDagBlocks()) {
    for (auto &blk : lvl.second) {
      // These are some sanity checks that difficulty is correct and block is truly non-finalized.
      // This is only done on startup
      auto period = db_->getDagBlockPeriod(blk->getHash());
      if (period != nullptr) {
        logger_->error("Nonfinalized Dag Block actually finalized in period {}", period->first);
        break;
      } else {
        auto propose_period = db_->getProposalPeriodForDagLevel(blk->getLevel());
        if (!propose_period.has_value()) {
          logger_->error(
              "No propose period for dag level {}"
              " found",
              blk->getLevel());
          assert(false);
          break;
        }

        const auto pk = key_manager_->getVrfKey(*propose_period, blk->getSender());
        if (!pk) {
          logger_->error(
              "DAG block {}"
              " with {} level is missing VRF key for sender {}",
              blk->getHash(), blk->getLevel(), blk->getSender());
          break;
        }
        // Verify VDF solution
        try {
          uint64_t max_vote_count = 0;
          const auto vote_count = final_chain_->dposEligibleVoteCount(*propose_period, blk->getSender());
          if (*propose_period < kGenesis.state.hardforks.magnolia_hf.block_num) {
            max_vote_count = final_chain_->dposEligibleTotalVoteCount(*propose_period);
          } else {
            max_vote_count = kValidatorMaxVote;
          }
          blk->verifyVdf(sortition_params_manager_.getSortitionParams(*propose_period),
                         db_->getPeriodBlockHash(*propose_period), *pk, vote_count, max_vote_count);
        } catch (vdf_sortition::VdfSortition::InvalidVdfSortition const &e) {
          logger_->error(
              "DAG block {}"
              " with {} level failed on VDF verification with pivot hash {} reason {}",
              blk->getHash(), blk->getLevel(), blk->getPivot(), e.what());
          break;
        }
      }

      // In case an invalid block somehow ended in DAG db, remove it
      auto res = pivotAndTipsAvailable(blk);
      if (res.first) {
        if (!addDagBlock(blk, {}, false, false).first) {
          logger_->error(
              "DAG block {}"
              " could not be added to DAG on startup, removing from db",
              blk->getHash());
          db_->removeDagBlock(blk->getHash());
        }
      } else {
        logger_->error("DAG block {} could not be added to DAG on startup since it has missing tip/pivot",
                       blk->getHash());
        db_->removeDagBlock(blk->getHash());
      }
    }
  }
  trx_mgr_->recoverNonfinalizedTransactions();
  updateFrontier();
}

const std::pair<PbftPeriod, std::map<uint64_t, std::unordered_set<blk_hash_t>>> DagManager::getNonFinalizedBlocks()
    const {
  std::shared_lock lock(mutex_);
  return {period_, non_finalized_blks_};
}

const std::tuple<PbftPeriod, std::vector<std::shared_ptr<DagBlock>>, SharedTransactions>
DagManager::getNonFinalizedBlocksWithTransactions(const std::unordered_set<blk_hash_t> &known_hashes) const {
  std::shared_lock lock(mutex_);
  std::vector<std::shared_ptr<DagBlock>> dag_blocks;
  std::unordered_set<trx_hash_t> unique_trxs;
  std::vector<trx_hash_t> trx_to_query;
  for (const auto &level_blocks : non_finalized_blks_) {
    for (const auto &hash : level_blocks.second) {
      if (known_hashes.count(hash) == 0) {
        if (auto blk = getDagBlock(hash); blk) {
          dag_blocks.emplace_back(blk);
        } else {
          logger_->error(
              "NonFinalizedBlock {}"
              " not in DB",
              hash);
          assert(false);
        }
      }
    }
  }
  for (const auto &block : dag_blocks) {
    for (auto trx : block->getTrxs()) {
      if (unique_trxs.emplace(trx).second) {
        trx_to_query.emplace_back(trx);
      }
    }
  }
  auto trxs = trx_mgr_->getNonfinalizedTrx(trx_to_query);
  return {period_, std::move(dag_blocks), std::move(trxs)};
}

uint32_t DagManager::getNonFinalizedBlocksMinDifficulty() const {
  std::shared_lock lock(mutex_);
  return non_finalized_blks_min_difficulty_;
}

std::pair<size_t, size_t> DagManager::getNonFinalizedBlocksSize() const {
  std::shared_lock lock(mutex_);

  size_t blocks_counter = 0;
  for (auto it = non_finalized_blks_.begin(); it != non_finalized_blks_.end(); ++it) {
    blocks_counter += it->second.size();
  }

  return {non_finalized_blks_.size(), blocks_counter};
}

std::pair<DagManager::VerifyBlockReturnType, SharedTransactions> DagManager::verifyBlock(
    const std::shared_ptr<DagBlock> &blk, const std::unordered_map<trx_hash_t, std::shared_ptr<Transaction>> &trxs) {
  const auto &block_hash = blk->getHash();
  vec_trx_t const &all_block_trx_hashes = blk->getTrxs();
  vec_trx_t trx_hashes_to_query;
  SharedTransactions all_block_trxs;

  // Verify tips/pivot count and uniqueness
  std::unordered_set<blk_hash_t> unique_tips_pivot;

  unique_tips_pivot.insert(blk->getPivot());
  if (blk->getTips().size() > kDagBlockMaxTips) {
    logger_->error(
        "DAG Block {}"
        " tips count {}"
        " over the limit",
        block_hash, blk->getTips().size());
    return {VerifyBlockReturnType::FailedTipsVerification, {}};
  }

  for (auto const &tip : blk->getTips()) {
    if (!unique_tips_pivot.insert(tip).second) {
      logger_->error(
          "DAG Block {}"
          " tip {}"
          " duplicate",
          block_hash, tip);
      return {VerifyBlockReturnType::FailedTipsVerification, {}};
    }
  }

  auto propose_period = db_->getProposalPeriodForDagLevel(blk->getLevel());

  // Verify DPOS
  if (!propose_period.has_value()) {
    // Cannot find the proposal period in DB yet. The slow node gets an ahead block, remove from seen_blocks
    logger_->info("Cannot find proposal period in DB for DAG block {}", blk->getHash());
    seen_blocks_.erase(block_hash);
    return {VerifyBlockReturnType::AheadBlock, {}};
  }

  if (trxs.size() != 0) {
    for (auto const &tx_hash : all_block_trx_hashes) {
      auto trx_it = trxs.find(tx_hash);
      if (trx_it != trxs.end()) {
        all_block_trxs.emplace_back(trx_it->second);
      } else {
        trx_hashes_to_query.emplace_back(tx_hash);
      }
    }
  } else {
    trx_hashes_to_query = all_block_trx_hashes;
  }

  // Verify transactions
  auto transactions = trx_mgr_->getTransactions(trx_hashes_to_query, *propose_period);

  if (transactions.size() < trx_hashes_to_query.size()) {
    logger_->info(
        "Ignore block {}"
        " since it has missing transactions",
        block_hash);
    // This can be a valid block so just remove it from the seen list
    seen_blocks_.erase(block_hash);
    return {VerifyBlockReturnType::MissingTransaction, {}};
  }

  for (auto t : transactions) {
    all_block_trxs.emplace_back(std::move(t));
  }

  if (blk->getLevel() < dag_expiry_level_) {
    logger_->info(
        "Dropping old block: {}"
        ". Expiry level: {}. Block level: {}",
        blk->getHash(), dag_expiry_level_, blk->getLevel());
    return {VerifyBlockReturnType::ExpiredBlock, {}};
  }

  // Verify VDF solution
  const auto pk = key_manager_->getVrfKey(*propose_period, blk->getSender());
  if (!pk) {
    logger_->error(
        "DAG block {}"
        " with {} level is missing VRF key for sender {}",
        blk->getHash(), blk->getLevel(), blk->getSender());
    return {VerifyBlockReturnType::FailedVdfVerification, {}};
  }

  try {
    const auto proposal_period_hash = db_->getPeriodBlockHash(*propose_period);
    uint64_t max_vote_count = 0;
    const auto vote_count = final_chain_->dposEligibleVoteCount(*propose_period, blk->getSender());
    if (*propose_period < kGenesis.state.hardforks.magnolia_hf.block_num) {
      max_vote_count = final_chain_->dposEligibleTotalVoteCount(*propose_period);
    } else {
      max_vote_count = kValidatorMaxVote;
    }
    blk->verifyVdf(sortition_params_manager_.getSortitionParams(*propose_period), proposal_period_hash, *pk, vote_count,
                   max_vote_count);
  } catch (vdf_sortition::VdfSortition::InvalidVdfSortition const &e) {
    logger_->error(
        "DAG block {}"
        " with {} level failed on VDF verification with pivot hash {}"
        " reason {}",
        block_hash, blk->getLevel(), blk->getPivot(), e.what());
    logger_->error(
        "period from map: {}"
        " current: {}",
        *propose_period, pbft_chain_->getPbftChainSize());
    return {VerifyBlockReturnType::FailedVdfVerification, {}};
  }

  auto dag_block_sender = blk->getSender();
  bool dpos_qualified;
  try {
    dpos_qualified = final_chain_->dposIsEligible(*propose_period, dag_block_sender);
  } catch (state_api::ErrFutureBlock &c) {
    logger_->error(
        "Verify proposal period {}"
        " is too far ahead of DPOS. {}",
        *propose_period, c.what());
    return {VerifyBlockReturnType::FutureBlock, {}};
  }
  if (!dpos_qualified) {
    logger_->error(
        "Invalid DAG block DPOS. DAG block {} is not eligible for DPOS at period {} for sender {} current period {}",
        blk->toString(), *propose_period, dag_block_sender.toString(), final_chain_->lastBlockNumber());
    return {VerifyBlockReturnType::NotEligible, {}};
  }
  {
    const auto [dag_gas_limit, pbft_gas_limit] = kGenesis.getGasLimits(*propose_period);

    auto block_gas_estimation = blk->getGasEstimation();
    if (block_gas_estimation > dag_gas_limit) {
      logger_->error("BlockTooBig. DAG block {} gas_limit: {} block_gas_estimation {} current period {}",
                     blk->getHash(), dag_gas_limit, block_gas_estimation, final_chain_->lastBlockNumber());
      return {VerifyBlockReturnType::BlockTooBig, {}};
    }
    auto total_block_weight = trx_mgr_->estimateTransactions(all_block_trxs, *propose_period);
    if (total_block_weight != block_gas_estimation) {
      logger_->error(
          "Invalid block_gas_estimation. DAG block {} block_gas_estimation: {}"
          " total_block_weight {} current period {}",
          blk->getHash(), block_gas_estimation, total_block_weight, final_chain_->lastBlockNumber());
      return {VerifyBlockReturnType::IncorrectTransactionsEstimation, {}};
    }

    if ((blk->getTips().size() + 1) > pbft_gas_limit / dag_gas_limit) {
      for (const auto &t : blk->getTips()) {
        const auto tip_blk = getDagBlock(t);
        if (tip_blk == nullptr) {
          logger_->error(
              "DAG Block {}"
              " tip {}"
              " not present",
              block_hash, t);
          return {VerifyBlockReturnType::MissingTip, {}};
        }
        block_gas_estimation += tip_blk->getGasEstimation();
      }
      if (block_gas_estimation > pbft_gas_limit) {
        logger_->error(
            "BlockTooBig. DAG block {}"
            " with tips has limit: {} block_gas_estimation {} current period {}",
            blk->getHash(), pbft_gas_limit, block_gas_estimation, final_chain_->lastBlockNumber());
        return {VerifyBlockReturnType::BlockTooBig, {}};
      }
    }
  }

  logger_->debug("Verified DAG block {}", blk->getHash());

  return {VerifyBlockReturnType::Verified, std::move(all_block_trxs)};
}

bool DagManager::isDagBlockKnown(const blk_hash_t &hash) const {
  auto known = seen_blocks_.count(hash);
  if (!known) {
    return db_->dagBlockInDb(hash);
  }
  return true;
}

std::shared_ptr<DagBlock> DagManager::getDagBlock(const blk_hash_t &hash) const {
  auto blk = seen_blocks_.get(hash);
  if (blk.second) {
    return blk.first;
  }
  if (hash == genesis_block_->getHash()) {
    return genesis_block_;
  }

  return db_->getDagBlock(hash);
}

dev::bytes DagManager::getVdfMessage(blk_hash_t const &hash, SharedTransactions const &trxs) {
  dev::RLPStream s;
  s << hash;
  for (const auto &t : trxs) {
    s << t->getHash();
  }
  return s.invalidate();
}

dev::bytes DagManager::getVdfMessage(blk_hash_t const &hash, std::vector<trx_hash_t> const &trx_hashes) {
  dev::RLPStream s;
  s << hash;
  for (const auto &h : trx_hashes) {
    s << h;
  }
  return s.invalidate();
}

}  // namespace taraxa
