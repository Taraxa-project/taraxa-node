#include "dag/dag_manager.hpp"

#include <libdevcore/CommonIO.h>

#include <algorithm>
#include <fstream>
#include <queue>
#include <stack>
#include <tuple>
#include <unordered_set>
#include <utility>
#include <vector>

#include "dag/dag.hpp"
#include "key_manager/key_manager.hpp"
#include "network/network.hpp"
#include "network/tarcap/packets_handlers/dag_block_packet_handler.hpp"
#include "transaction/transaction_manager.hpp"

namespace taraxa {
DagManager::DagManager(blk_hash_t const &dag_genesis_block_hash, addr_t node_addr,
                       const SortitionConfig &sortition_config, const DagConfig &dag_config,
                       std::shared_ptr<TransactionManager> trx_mgr, std::shared_ptr<PbftChain> pbft_chain,
                       std::shared_ptr<FinalChain> final_chain, std::shared_ptr<DbStorage> db,
                       std::shared_ptr<KeyManager> key_manager, bool is_light_node, uint64_t light_node_history,
                       uint32_t max_levels_per_period, uint32_t dag_expiry_limit) try
    : pivot_tree_(std::make_shared<PivotTree>(dag_genesis_block_hash, node_addr)),
      total_dag_(std::make_shared<Dag>(dag_genesis_block_hash, node_addr)),
      trx_mgr_(trx_mgr),
      pbft_chain_(pbft_chain),
      db_(db),
      key_manager_(std::move(key_manager)),
      anchor_(dag_genesis_block_hash),
      period_(0),
      sortition_params_manager_(node_addr, sortition_config, db_),
      dag_config_(dag_config),
      is_light_node_(is_light_node),
      light_node_history_(light_node_history),
      max_levels_per_period_(max_levels_per_period),
      dag_expiry_limit_(dag_expiry_limit),
      seen_blocks_(cache_max_size_, cache_delete_step_),
      final_chain_(final_chain) {
  LOG_OBJECTS_CREATE("DAGMGR");
  if (auto ret = getLatestPivotAndTips(); ret) {
    frontier_.pivot = ret->first;
    for (const auto &t : ret->second) {
      frontier_.tips.push_back(t);
    }
  }
  // Set DAG level proposal period map
  if (!db_->getProposalPeriodForDagLevel(max_levels_per_period)) {
    // Node start from scratch
    db_->saveProposalPeriodDagLevelsMap(max_levels_per_period, 0);
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
  SharedLock lock(mutex_);
  return {db_->getNumDagBlocks(), total_dag_->getNumVertices()};
}

std::pair<uint64_t, uint64_t> DagManager::getNumEdgesInDag() const {
  SharedLock lock(mutex_);
  return {db_->getDagEdgeCount(), total_dag_->getNumEdges()};
}

void DagManager::drawTotalGraph(std::string const &str) const {
  SharedLock lock(mutex_);
  total_dag_->drawGraph(str);
}

void DagManager::drawPivotGraph(std::string const &str) const {
  SharedLock lock(mutex_);
  pivot_tree_->drawGraph(str);
}

std::pair<bool, std::vector<blk_hash_t>> DagManager::pivotAndTipsAvailable(DagBlock const &blk) {
  auto dag_blk_hash = blk.getHash();
  const auto pivot_hash = blk.getPivot();
  const auto dag_blk_pivot = getDagBlock(pivot_hash);
  std::vector<blk_hash_t> missing_tips_or_pivot;

  level_t expected_level = 0;
  if (dag_blk_pivot) {
    expected_level = dag_blk_pivot->getLevel() + 1;
  } else {
    LOG(log_nf_) << "DAG Block " << dag_blk_hash.toString() << " pivot " << pivot_hash.toString() << " unavailable";
    missing_tips_or_pivot.push_back(pivot_hash);
  }

  for (auto const &tip : blk.getTips()) {
    auto tip_block = getDagBlock(tip);
    if (tip_block) {
      expected_level = std::max(expected_level, tip_block->getLevel() + 1);
    } else {
      LOG(log_nf_) << "DAG Block " << dag_blk_hash << " tip " << tip << " unavailable";
      missing_tips_or_pivot.push_back(tip);
    }
  }

  if (missing_tips_or_pivot.size() > 0) {
    return {false, missing_tips_or_pivot};
  }

  if (expected_level != blk.getLevel()) {
    LOG(log_er_) << "DAG Block " << dag_blk_hash << " level " << blk.getLevel()
                 << ", expected level: " << expected_level;
    return {false, missing_tips_or_pivot};
  }

  return {true, missing_tips_or_pivot};
}

DagFrontier DagManager::getDagFrontier() {
  SharedLock lock(mutex_);
  return frontier_;
}

std::pair<bool, std::vector<blk_hash_t>> DagManager::addDagBlock(DagBlock &&blk, SharedTransactions &&trxs,
                                                                 bool proposed, bool save) {
  auto blk_hash = blk.getHash();

  {
    // One mutex protects the DagManager internal state, the other mutex ensures that dag blocks are gossiped in
    // correct order since multiple threads can call this method. There is a need for using two mutexes since having
    // blocks gossip under mutex_ leads to a deadlock with mutex in TaraxaPeer
    ULock order_lock(order_dag_blocks_mutex_);
    {
      ULock lock(mutex_);
      if (save) {
        if (db_->dagBlockInDb(blk.getHash())) {
          // It is a valid scenario that two threads can receive same block from two peers and process at same time
          return {true, {}};
        }

        if (blk.getLevel() < dag_expiry_level_) {
          LOG(log_nf_) << "Dropping old block: " << blk_hash << ". Expiry level: " << dag_expiry_level_
                       << ". Block level: " << blk.getLevel();
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
      seen_blocks_.insert(blk.getHash(), blk);
      auto pivot_hash = blk.getPivot();

      std::vector<blk_hash_t> tips = blk.getTips();
      level_t current_max_level = max_level_;
      max_level_ = std::max(current_max_level, blk.getLevel());

      addToDag(blk_hash, pivot_hash, tips, blk.getLevel());

      updateFrontier();
    }
    if (save) {
      block_verified_.emit(blk);
      if (auto net = network_.lock()) {
        net->getSpecificHandler<network::tarcap::DagBlockPacketHandler>()->onNewBlockVerified(std::move(blk), proposed,
                                                                                              std::move(trxs));
      }
    }
  }
  LOG(log_nf_) << " Update frontier after adding block " << blk_hash << "anchor " << anchor_
               << " pivot = " << frontier_.pivot << " tips: " << frontier_.tips;
  return {true, {}};
}

void DagManager::drawGraph(std::string const &dotfile) const {
  SharedLock lock(mutex_);
  drawPivotGraph("pivot." + dotfile);
  drawTotalGraph("total." + dotfile);
}

void DagManager::addToDag(blk_hash_t const &hash, blk_hash_t const &pivot, std::vector<blk_hash_t> const &tips,
                          uint64_t level, bool finalized) {
  total_dag_->addVEEs(hash, pivot, tips);
  pivot_tree_->addVEEs(hash, pivot, {});

  LOG(log_dg_) << " Insert block to DAG : " << hash;
  if (finalized) {
    return;
  }

  if (!non_finalized_blks_[level].insert(hash).second) {
    LOG(log_er_) << "Trying to insert duplicate block into the dag: " << hash;
  }
}

std::optional<std::pair<blk_hash_t, std::vector<blk_hash_t>>> DagManager::getLatestPivotAndTips() const {
  SharedLock lock(mutex_);
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

  SharedLock lock(mutex_);
  return pivot_tree_->getGhostPath(source);
}

std::vector<blk_hash_t> DagManager::getGhostPath() const {
  SharedLock lock(mutex_);
  auto last_pivot = anchor_;
  return pivot_tree_->getGhostPath(last_pivot);
}

// return {block order}, for pbft-pivot-blk proposing
std::vector<blk_hash_t> DagManager::getDagBlockOrder(blk_hash_t const &anchor, PbftPeriod period) {
  SharedLock lock(mutex_);
  std::vector<blk_hash_t> blk_orders;

  if (period != period_ + 1) {
    LOG(log_wr_) << "getDagBlockOrder called with period " << period << ". Expected period " << period_ + 1;
    return {};
  }

  if (anchor_ == anchor) {
    LOG(log_wr_) << "Query period from " << anchor_ << " to " << anchor << " not ok " << std::endl;
    return {};
  }

  auto new_period = period_ + 1;

  auto ok = total_dag_->computeOrder(anchor, blk_orders, non_finalized_blks_);
  if (!ok) {
    LOG(log_er_) << " Create period " << new_period << " anchor: " << anchor << " failed " << std::endl;
    return {};
  }

  LOG(log_dg_) << "Get period " << new_period << " from " << anchor_ << " to " << anchor << " with "
               << blk_orders.size() << " blks" << std::endl;

  return blk_orders;
}

void DagManager::clearLightNodeHistory() {
  // Actual history size will be between 100% and 110% of light_node_history_ to avoid deleting on every period
  if (((period_ % (std::max(light_node_history_ / 10, (uint64_t)1)) == 0)) && period_ > light_node_history_ &&
      dag_expiry_level_ > max_levels_per_period_ + 1) {
    const auto proposal_period = db_->getProposalPeriodForDagLevel(dag_expiry_level_ - max_levels_per_period_ - 1);
    assert(proposal_period);

    const uint64_t start = 0;
    // This prevents deleting any data needed for dag blocks proposal period, we only delete periods for the expired dag
    // blocks
    const uint64_t end = std::min(period_ - light_node_history_, *proposal_period);
    LOG(log_tr_) << "period_ - light_node_history_ " << period_ - light_node_history_;
    LOG(log_tr_) << "dag_expiry_level - max_levels_per_period_ - 1: " << dag_expiry_level_ - max_levels_per_period_ - 1
                 << " *proposal_period " << *proposal_period;
    LOG(log_tr_) << "Delete period history from: " << start << " to " << end;
    db_->clearPeriodDataHistory(end);
  }
}

uint DagManager::setDagBlockOrder(blk_hash_t const &new_anchor, PbftPeriod period, vec_blk_t const &dag_order) {
  LOG(log_dg_) << "setDagBlockOrder called with anchor " << new_anchor << " and period " << period;
  if (period != period_ + 1) {
    LOG(log_er_) << " Inserting period (" << period << ") anchor " << new_anchor
                 << " does not match ..., previous internal period (" << period_ << ") " << anchor_;
    return 0;
  }

  if (new_anchor == kNullBlockHash) {
    period_ = period;
    LOG(log_nf_) << "Set new period " << period << " with kNullBlockHash anchor";
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
  std::vector<DagBlock> dag_blocks_to_update_counters;
  for (auto const &blk : dag_order) {
    if (non_finalized_blocks_set.count(blk) == 0) {
      auto dag_block = getDagBlock(blk);
      dag_blocks_to_update_counters.push_back(*dag_block);
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

  for (auto &v : non_finalized_blocks) {
    for (auto &blk_hash : v.second) {
      if (dag_order_set.count(blk_hash) != 0) {
        continue;
      }

      auto dag_block = getDagBlock(blk_hash);
      auto pivot_hash = dag_block->getPivot();

      if (validateBlockNotExpired(dag_block, expired_dag_blocks_to_remove)) {
        addToDag(blk_hash, pivot_hash, dag_block->getTips(), dag_block->getLevel(), false);
      } else {
        db_->removeDagBlock(blk_hash);
        seen_blocks_.erase(blk_hash);
        for (const auto &trx : dag_block->getTrxs()) expired_dag_blocks_transactions.emplace_back(trx);
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
  updateFrontier();

  if (is_light_node_) {
    clearLightNodeHistory();
  }

  LOG(log_nf_) << "Set new period " << period << " with anchor " << new_anchor;

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
    trx_mgr_->moveNonFinalizedTransactionsToTransactionsPool(std::move(transactions_from_expired_dag_blocks_to_remove));
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
    LOG(log_nf_) << "Dropping expired block in setDagBlockOrder: " << blk_hash
                 << ". Expiry level: " << dag_expiry_level_ << ". Block level: " << dag_block->getLevel();
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
        LOG(log_nf_) << "Recover anchor " << anchor_;
        addToDag(anchor_, kNullBlockHash, vec_blk_t(), 0, true);
        break;
      }
    }

    if (pbft_block_hash) {
      auto pbft_block = pbft_chain_->getPbftBlockInChain(pbft_block_hash);
      auto anchor = pbft_block.getPivotDagBlockHash();
      if (anchor) {
        old_anchor_ = anchor;
        LOG(log_nf_) << "Recover old anchor " << old_anchor_;
      }
    }
  }

  for (auto &lvl : db_->getNonfinalizedDagBlocks()) {
    for (auto &blk : lvl.second) {
      // These are some sanity checks that difficulty is correct and block is truly non-finalized.
      // This is only done on startup
      auto period = db_->getDagBlockPeriod(blk.getHash());
      if (period != nullptr) {
        LOG(log_er_) << "Nonfinalized Dag Block actually finalized in period " << period->first;
        break;
      } else {
        auto propose_period = db_->getProposalPeriodForDagLevel(blk.getLevel());
        if (!propose_period.has_value()) {
          LOG(log_er_) << "No propose period for dag level " << blk.getLevel() << " found";
          assert(false);
          break;
        }

        const auto pk = key_manager_->get(*propose_period, blk.getSender());
        if (!pk) {
          LOG(log_er_) << "DAG block " << blk.getHash() << " with " << blk.getLevel()
                       << " level is missing VRF key for sender " << blk.getSender();
          break;
        }
        // Verify VDF solution
        try {
          blk.verifyVdf(sortition_params_manager_.getSortitionParams(*propose_period),
                        db_->getPeriodBlockHash(*propose_period), *pk);
        } catch (vdf_sortition::VdfSortition::InvalidVdfSortition const &e) {
          LOG(log_er_) << "DAG block " << blk.getHash() << " with " << blk.getLevel()
                       << " level failed on VDF verification with pivot hash " << blk.getPivot() << " reason "
                       << e.what();
          break;
        }
      }

      // In case an invalid block somehow ended in DAG db, remove it
      auto res = pivotAndTipsAvailable(blk);
      if (res.first) {
        if (!addDagBlock(std::move(blk), {}, false, false).first) {
          LOG(log_er_) << "DAG block " << blk.getHash() << " could not be added to DAG on startup, removing from db";
          db_->removeDagBlock(blk.getHash());
        }
      } else {
        LOG(log_er_) << "DAG block " << blk.getHash()
                     << " could not be added to DAG on startup since it has missing tip/pivot";
        db_->removeDagBlock(blk.getHash());
      }
    }
  }
  trx_mgr_->recoverNonfinalizedTransactions();
}

const std::pair<PbftPeriod, std::map<uint64_t, std::unordered_set<blk_hash_t>>> DagManager::getNonFinalizedBlocks()
    const {
  SharedLock lock(mutex_);
  return {period_, non_finalized_blks_};
}

const std::tuple<PbftPeriod, std::vector<std::shared_ptr<DagBlock>>, SharedTransactions>
DagManager::getNonFinalizedBlocksWithTransactions(const std::unordered_set<blk_hash_t> &known_hashes) const {
  SharedLock lock(mutex_);
  std::vector<std::shared_ptr<DagBlock>> dag_blocks;
  std::unordered_set<trx_hash_t> unique_trxs;
  std::vector<trx_hash_t> trx_to_query;
  for (const auto &level_blocks : non_finalized_blks_) {
    for (const auto &hash : level_blocks.second) {
      if (known_hashes.count(hash) == 0) {
        if (auto blk = getDagBlock(hash); blk) {
          dag_blocks.emplace_back(blk);
        } else {
          LOG(log_er_) << "NonFinalizedBlock " << hash << " not in DB";
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

std::pair<size_t, size_t> DagManager::getNonFinalizedBlocksSize() const {
  SharedLock lock(mutex_);

  size_t blocks_counter = 0;
  for (auto it = non_finalized_blks_.begin(); it != non_finalized_blks_.end(); ++it) {
    blocks_counter += it->second.size();
  }

  return {non_finalized_blks_.size(), blocks_counter};
}

DagManager::VerifyBlockReturnType DagManager::verifyBlock(const DagBlock &blk) {
  const auto &block_hash = blk.getHash();

  // Verify tips/pivot count amd uniqueness
  std::unordered_set<blk_hash_t> unique_tips_pivot;

  unique_tips_pivot.insert(blk.getPivot());
  if (blk.getTips().size() > kDagBlockMaxTips) {
    LOG(log_er_) << "DAG Block " << block_hash << " tips count " << blk.getTips().size() << " over the limit";
    return VerifyBlockReturnType::FailedTipsVerification;
  }

  for (auto const &tip : blk.getTips()) {
    if (!unique_tips_pivot.insert(tip).second) {
      LOG(log_er_) << "DAG Block " << block_hash << " tip " << tip << " duplicate";
      return VerifyBlockReturnType::FailedTipsVerification;
    }
  }

  // Verify transactions
  auto transactions = trx_mgr_->getBlockTransactions(blk);
  if (!transactions.has_value()) {
    LOG(log_nf_) << "Ignore block " << block_hash << " since it has missing transactions";
    // This can be a valid block so just remove it from the seen list
    seen_blocks_.erase(block_hash);
    return VerifyBlockReturnType::MissingTransaction;
  }

  auto propose_period = db_->getProposalPeriodForDagLevel(blk.getLevel());
  // Verify DPOS
  if (!propose_period.has_value()) {
    // Cannot find the proposal period in DB yet. The slow node gets an ahead block, remove from seen_blocks
    LOG(log_nf_) << "Cannot find proposal period in DB for DAG block " << blk.getHash();
    seen_blocks_.erase(block_hash);
    return VerifyBlockReturnType::AheadBlock;
  }

  if (blk.getLevel() < dag_expiry_level_) {
    LOG(log_nf_) << "Dropping old block: " << blk.getHash() << ". Expiry level: " << dag_expiry_level_
                 << ". Block level: " << blk.getLevel();
    return VerifyBlockReturnType::ExpiredBlock;
  }

  // Verify VDF solution
  const auto pk = key_manager_->get(*propose_period, blk.getSender());
  if (!pk) {
    LOG(log_er_) << "DAG block " << blk.getHash() << " with " << blk.getLevel()
                 << " level is missing VRF key for sender " << blk.getSender();
    return VerifyBlockReturnType::FailedVdfVerification;
  }

  try {
    const auto proposal_period_hash = db_->getPeriodBlockHash(*propose_period);
    blk.verifyVdf(sortition_params_manager_.getSortitionParams(*propose_period), proposal_period_hash, *pk);
  } catch (vdf_sortition::VdfSortition::InvalidVdfSortition const &e) {
    LOG(log_er_) << "DAG block " << block_hash << " with " << blk.getLevel()
                 << " level failed on VDF verification with pivot hash " << blk.getPivot() << " reason " << e.what();
    LOG(log_er_) << "period from map: " << *propose_period << " current: " << pbft_chain_->getPbftChainSize();
    return VerifyBlockReturnType::FailedVdfVerification;
  }

  auto dag_block_sender = blk.getSender();
  bool dpos_qualified;
  try {
    dpos_qualified = final_chain_->dpos_is_eligible(*propose_period, dag_block_sender);
  } catch (state_api::ErrFutureBlock &c) {
    LOG(log_er_) << "Verify proposal period " << *propose_period << " is too far ahead of DPOS. " << c.what();
    return VerifyBlockReturnType::FutureBlock;
  }
  if (!dpos_qualified) {
    LOG(log_er_) << "Invalid DAG block DPOS. DAG block " << blk << " is not eligible for DPOS at period "
                 << *propose_period << " for sender " << dag_block_sender.toString() << " current period "
                 << final_chain_->last_block_number();
    return VerifyBlockReturnType::NotEligible;
  }
  {
    u256 total_block_weight = 0;
    const auto &block_gas_estimation = blk.getGasEstimation();
    for (const auto &trx : *transactions) {
      total_block_weight += trx_mgr_->estimateTransactionGas(trx, propose_period);
    }

    if (total_block_weight != block_gas_estimation) {
      LOG(log_er_) << "Invalid block_gas_estimation. DAG block " << blk.getHash()
                   << " block_gas_estimation: " << block_gas_estimation << " total_block_weight " << total_block_weight
                   << " current period " << final_chain_->last_block_number();
      return VerifyBlockReturnType::IncorrectTransactionsEstimation;
    }

    if (total_block_weight > getDagConfig().gas_limit) {
      LOG(log_er_) << "BlockTooBig. DAG block " << blk.getHash() << " gas_limit: " << getDagConfig().gas_limit
                   << " total_block_weight " << total_block_weight << " current period "
                   << final_chain_->last_block_number();
      return VerifyBlockReturnType::BlockTooBig;
    }
  }

  LOG(log_dg_) << "Verified DAG block " << blk.getHash();

  return VerifyBlockReturnType::Verified;
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
    return std::make_shared<DagBlock>(blk.first);
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
