#include "dag/dag.hpp"

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
#include "network/network.hpp"
#include "transaction/transaction_manager.hpp"

#define NULL_BLOCK_HASH blk_hash_t(0)

namespace taraxa {

Dag::Dag(blk_hash_t const &genesis, addr_t node_addr) {
  LOG_OBJECTS_CREATE("DAGMGR");
  std::vector<blk_hash_t> tips;
  // add genesis block
  addVEEs(genesis, {}, tips);
}

uint64_t Dag::getNumVertices() const { return boost::num_vertices(graph_); }
uint64_t Dag::getNumEdges() const { return boost::num_edges(graph_); }

bool Dag::hasVertex(blk_hash_t const &v) const { return graph_.vertex(v) != graph_.null_vertex(); }

void Dag::getLeaves(std::vector<blk_hash_t> &tips) const {
  vertex_index_map_const_t index_map = boost::get(boost::vertex_index, graph_);
  std::vector<vertex_t> leaves;
  collectLeafVertices(leaves);
  std::transform(leaves.begin(), leaves.end(), std::back_inserter(tips),
                 [index_map](const vertex_t &leaf) { return index_map[leaf]; });
}

bool Dag::addVEEs(blk_hash_t const &new_vertex, blk_hash_t const &pivot, std::vector<blk_hash_t> const &tips) {
  assert(!new_vertex.isZero());

  // add vertex
  vertex_t ret = add_vertex(new_vertex, graph_);
  boost::get(boost::vertex_index, graph_)[ret] = new_vertex;
  // TODO do we need this?
  // edge_index_map_t weight_map = boost::get(boost::edge_index, graph_);

  edge_t edge;
  bool res = true;

  // Note: add edges,
  // *** important
  // Add a new block, edges are pointing from pivot to new_veretx
  if (!pivot.isZero()) {
    if (hasVertex(pivot)) {
      std::tie(edge, res) = boost::add_edge_by_label(pivot, new_vertex, graph_);
      // TODO do we need this?
      // weight_map[edge] = 1;
      if (!res) {
        LOG(log_wr_) << "Creating pivot edge \n" << pivot << "\n-->\n" << new_vertex << " \nunsuccessful!" << std::endl;
      }
    }
  }
  bool res2 = true;
  for (auto const &e : tips) {
    if (hasVertex(e)) {
      std::tie(edge, res2) = boost::add_edge_by_label(e, new_vertex, graph_);
      // TODO do we need this?
      // weight_map[edge] = 0;
      if (!res2) {
        LOG(log_wr_) << "Creating tip edge \n" << e << "\n-->\n" << new_vertex << " \nunsuccessful!" << std::endl;
      }
    }
  }
  res &= res2;
  return res;
}

void Dag::drawGraph(std::string const &filename) const {
  std::ofstream outfile(filename.c_str());
  auto index_map = boost::get(boost::vertex_index, graph_);
  auto weight_map = boost::get(boost::edge_index, graph_);

  boost::write_graphviz(outfile, graph_, vertex_label_writer(index_map), edge_label_writer(weight_map));
  std::cout << "Dot file " << filename << " generated!" << std::endl;
  std::cout << "Use \"dot -Tpdf <dot file> -o <pdf file>\" to generate pdf file" << std::endl;
}

void Dag::clear() { graph_ = graph_t(); }

void Dag::collectLeafVertices(std::vector<vertex_t> &leaves) const {
  leaves.clear();
  vertex_iter_t s, e;
  // iterator all vertex
  for (std::tie(s, e) = boost::vertices(graph_); s != e; ++s) {
    // if out-degree zero, leaf node
    if (boost::out_degree(*s, graph_) == 0) {
      leaves.emplace_back(*s);
    }
  }
  assert(leaves.size());
}

// only iterate through non finalized blocks
bool Dag::computeOrder(const blk_hash_t &anchor, std::vector<blk_hash_t> &ordered_period_vertices,
                       const std::map<uint64_t, std::unordered_set<blk_hash_t>> &non_finalized_blks) {
  vertex_t target = graph_.vertex(anchor);

  if (target == graph_.null_vertex()) {
    LOG(log_wr_) << "Dag::ComputeOrder cannot find vertex (anchor) " << anchor << "\n";
    return false;
  }
  ordered_period_vertices.clear();

  vertex_iter_t s, e;
  vertex_index_map_t index_map = boost::get(boost::vertex_index, graph_);  // from vertex_descriptor to hash
  std::map<blk_hash_t, vertex_t> epfriend;                                 // this is unordered epoch
  epfriend[index_map[target]] = target;

  // Step 1: collect all epoch blks that can reach anchor
  // Erase from recent_added_blks after mark epoch number if finalized

  for (auto &l : non_finalized_blks) {
    for (auto &blk : l.second) {
      auto v = graph_.vertex(blk);
      if (reachable(v, target)) {
        epfriend[index_map[v]] = v;
      }
    }
  }
  // Step2: compute topological order of epfriend
  std::unordered_set<vertex_t> visited;
  std::stack<std::pair<vertex_t, bool>> dfs;
  vertex_adj_iter_t adj_s, adj_e;

  for (auto const &vp : epfriend) {
    auto const &v = vp.second;
    if (visited.count(v)) {
      continue;
    }
    dfs.push({v, false});
    visited.insert(v);
    while (!dfs.empty()) {
      auto cur = dfs.top();
      dfs.pop();
      if (cur.second) {
        ordered_period_vertices.emplace_back(index_map[cur.first]);
        continue;
      }
      dfs.push({cur.first, true});
      std::vector<std::pair<blk_hash_t, vertex_t>> neighbors;
      // iterate through neighbors
      for (std::tie(adj_s, adj_e) = adjacenct_vertices(cur.first, graph_); adj_s != adj_e; adj_s++) {
        if (epfriend.find(index_map[*adj_s]) == epfriend.end()) {  // not in this epoch
          continue;
        }
        if (visited.count(*adj_s)) {
          continue;
        }
        neighbors.emplace_back(std::make_pair(index_map[*adj_s], *adj_s));
        visited.insert(*adj_s);
      }
      // make sure iterated nodes have deterministic order
      std::sort(neighbors.begin(), neighbors.end());
      for (auto const &n : neighbors) {
        dfs.push({n.second, false});
      }
    }
  }
  std::reverse(ordered_period_vertices.begin(), ordered_period_vertices.end());
  return true;
}

// dfs
bool Dag::reachable(vertex_t const &from, vertex_t const &to) const {
  if (from == to) return true;
  vertex_t current = from;
  vertex_t target = to;
  std::stack<vertex_t> st;
  std::set<vertex_t> visited;
  st.push(current);
  visited.insert(current);

  while (!st.empty()) {
    vertex_t t = st.top();
    st.pop();
    vertex_adj_iter_t s, e;
    for (std::tie(s, e) = adjacenct_vertices(t, graph_); s != e; ++s) {
      if (visited.count(*s)) continue;
      if (*s == target) return true;
      visited.insert(*s);
      st.push(*s);
    }
  }
  return false;
}

/**
 * Iterative version
 * Steps rounds
 * 1. post order traversal
 * 2. from leave, count weight and propagate up
 * 3. collect path
 */

void PivotTree::getGhostPath(blk_hash_t const &vertex, std::vector<blk_hash_t> &pivot_chain) const {
  std::vector<vertex_t> post_order;
  vertex_t root = graph_.vertex(vertex);

  if (root == graph_.null_vertex()) {
    LOG(log_wr_) << "Cannot find vertex (getGhostPath) " << vertex << std::endl;
    return;
  }
  pivot_chain.clear();

  // first step: post order traversal
  std::stack<vertex_t> st;
  st.emplace(root);
  vertex_t cur;
  vertex_adj_iter_t s, e;
  while (!st.empty()) {
    cur = st.top();
    st.pop();
    post_order.emplace_back(cur);
    for (std::tie(s, e) = adjacenct_vertices(cur, graph_); s != e; s++) {
      st.emplace(*s);
    }
  }
  std::reverse(post_order.begin(), post_order.end());

  // second step: compute weight based on step one
  std::unordered_map<vertex_t, size_t> weight_map;
  for (auto const &n : post_order) {
    auto total_w = 0;
    // get childrens
    for (std::tie(s, e) = adjacenct_vertices(n, graph_); s != e; s++) {
      if (weight_map.count(*s)) {  // bigger timestamp
        total_w += weight_map[*s];
      }
    }
    weight_map[n] = total_w + 1;
  }

  vertex_index_map_const_t index_map = boost::get(boost::vertex_index, graph_);

  // third step: collect path
  while (1) {
    pivot_chain.emplace_back(index_map[root]);
    size_t heavist = 0;
    vertex_t next = root;

    for (std::tie(s, e) = adjacenct_vertices(root, graph_); s != e; s++) {
      if (!weight_map.count(*s)) continue;  // bigger timestamp
      size_t w = weight_map[*s];
      assert(w > 0);
      if (w > heavist) {
        heavist = w;
        next = *s;
      } else if (w == heavist) {
        if (index_map[*s] < index_map[next]) {
          heavist = w;
          next = *s;
        }
      }
    }
    if (heavist == 0)
      break;
    else
      root = next;
  }
}

DagManager::DagManager(blk_hash_t const &genesis, addr_t node_addr, std::shared_ptr<TransactionManager> trx_mgr,
                       std::shared_ptr<PbftChain> pbft_chain, std::shared_ptr<DagBlockManager> dag_blk_mgr,
                       std::shared_ptr<DbStorage> db, logger::Logger log_time, bool is_light_node,
                       uint64_t light_node_history, uint32_t max_levels_per_period, uint32_t dag_expiry_limit) try
    : pivot_tree_(std::make_shared<PivotTree>(genesis, node_addr)),
      total_dag_(std::make_shared<Dag>(genesis, node_addr)),
      trx_mgr_(trx_mgr),
      pbft_chain_(pbft_chain),
      dag_blk_mgr_(dag_blk_mgr),
      db_(db),
      anchor_(genesis),
      period_(0),
      genesis_(genesis),
      is_light_node_(is_light_node),
      light_node_history_(light_node_history),
      max_levels_per_period_(max_levels_per_period),
      dag_expiry_limit_(dag_expiry_limit),
      log_time_(log_time) {
  LOG_OBJECTS_CREATE("DAGMGR");
  if (auto ret = getLatestPivotAndTips(); ret) {
    frontier_.pivot = ret->first;
    for (const auto &t : ret->second) {
      frontier_.tips.push_back(t);
    }
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

void DagManager::stop() {
  if (bool b = false; !stopped_.compare_exchange_strong(b, !b)) {
    return;
  }
  std::unique_lock lock(mutex_);
  block_worker_.join();
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

bool DagManager::pivotAndTipsAvailable(DagBlock const &blk) {
  auto dag_blk_hash = blk.getHash();
  auto dag_blk_pivot = blk.getPivot();

  if (!db_->dagBlockInDb(dag_blk_pivot)) {
    LOG(log_nf_) << "DAG Block " << dag_blk_hash << " pivot " << dag_blk_pivot << " unavailable";
    return false;
  }

  for (auto const &t : blk.getTips()) {
    if (!db_->dagBlockInDb(t)) {
      LOG(log_nf_) << "DAG Block " << dag_blk_hash << " tip " << t << " unavailable";
      return false;
    }
  }

  return true;
}

DagFrontier DagManager::getDagFrontier() {
  SharedLock lock(mutex_);
  return frontier_;
}

void DagManager::start() {
  if (bool b = true; !stopped_.compare_exchange_strong(b, !b)) {
    return;
  }
  block_worker_ = std::thread([this]() { worker(); });
}

void DagManager::worker() {
  bool level_limit = false;
  uint64_t level = 0;
  while (!stopped_) {
    // will block if no verified block available
    auto blk = dag_blk_mgr_->popVerifiedBlock(level_limit, level);
    if (blk == std::nullopt) {
      continue;
    }
    level_limit = false;
    if (pivotAndTipsAvailable(*blk)) {
      // Retrieve pool transactions
      auto [transactions, missing_trxs] = trx_mgr_->getPoolTransactions(blk->getTrxs());

      // DAG block should not have been verified if it is missing a transaction
      // This check is taking some resources so in time we can remove it but it is safe to have it as a sanity check
      size_t trx_found_count = 0;
      for (const auto b : db_->transactionsInDb(missing_trxs)) {
        if (b) trx_found_count++;
      }
      assert(missing_trxs.size() == trx_found_count);

      addDagBlock(std::move(blk.value()), std::move(transactions));
    } else {
      // Networking makes sure that dag block that reaches queue already had
      // its pivot and tips processed This should happen in a very rare case
      // where in some race condition older block is verfified faster then
      // new block but should resolve quickly, return block to queue
      if (!stopped_) {
        if (dag_blk_mgr_->pivotAndTipsValid(*blk)) {
          dag_blk_mgr_->pushVerifiedBlock(*blk);
          level_limit = true;
          level = blk->getLevel();
        }
      }
    }
  }
}

bool DagManager::addDagBlock(DagBlock &&blk, SharedTransactions &&trxs, bool proposed, bool save) {
  auto blk_hash = blk.getHash();

  {
    // One mutex protects the DagManager internal state, the other mutex ensures that dag blocks are gossiped in
    // correct order since multiple threads can call this method. There is a need for using two mutexes since having
    // blocks gossip under mutex_ leads to a deadlock with mutex in TaraxaPeer
    ULock order_lock(order_dag_blocks_mutex_);
    {
      ULock lock(mutex_);
      if (save) {
        if (db_->dagBlockInDb(blk_hash)) {
          LOG(log_dg_) << "Block already in DB: " << blk_hash;
          return false;
        }
        const auto dag_expiry_level = dag_blk_mgr_->getDagExpiryLevel();
        if (blk.getLevel() < dag_expiry_level) {
          LOG(log_nf_) << "Dropping old block: " << blk_hash << ". Expiry level: " << dag_expiry_level
                       << ". Block level: " << blk.getLevel();
          return false;
        }
        // Saves transactions and remove them from memory pool
        trx_mgr_->saveTransactionsFromDagBlock(trxs);
        // Save the dag block
        db_->saveDagBlock(blk);
      }
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
        net->onNewBlockVerified(std::move(blk), proposed, std::move(trxs));
      }
    }
  }
  LOG(log_nf_) << " Update frontier after adding block " << blk_hash << "anchor " << anchor_
               << " pivot = " << frontier_.pivot << " tips: " << frontier_.tips;
  return true;
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
  std::vector<blk_hash_t> pivot_chain;

  auto last_pivot = anchor_;
  pivot_tree_->getGhostPath(last_pivot, pivot_chain);
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

void DagManager::getGhostPath(blk_hash_t const &source, std::vector<blk_hash_t> &ghost) const {
  SharedLock lock(mutex_);
  pivot_tree_->getGhostPath(source, ghost);
}

void DagManager::getGhostPath(std::vector<blk_hash_t> &ghost) const {
  SharedLock lock(mutex_);
  auto last_pivot = anchor_;
  ghost.clear();
  pivot_tree_->getGhostPath(last_pivot, ghost);
}

// return {block order}, for pbft-pivot-blk proposing
std::vector<blk_hash_t> DagManager::getDagBlockOrder(blk_hash_t const &anchor, uint64_t period) {
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
  const auto dag_expiry_level = dag_blk_mgr_->getDagExpiryLevel();
  // Actual history size will be between 100% and 110% of light_node_history_ to avoid deleting on every period
  if (((period_ % (std::max(light_node_history_ / 10, (uint64_t)1)) == 0)) && period_ > light_node_history_ &&
      dag_expiry_level > max_levels_per_period_ + 1) {
    const auto proposal_period = db_->getProposalPeriodForDagLevel(dag_expiry_level - max_levels_per_period_ - 1);
    assert(proposal_period);

    const uint64_t start = 0;
    // This prevents deleting any data needed for dag blocks proposal period, we only delete periods for the expired dag
    // blocks
    const uint64_t end = std::min(period_ - light_node_history_, *proposal_period);
    LOG(log_tr_) << "period_ - light_node_history_ " << period_ - light_node_history_;
    LOG(log_tr_) << "dag_expiry_level - max_levels_per_period_ - 1: " << dag_expiry_level - max_levels_per_period_ - 1
                 << " *proposal_period " << *proposal_period;
    LOG(log_tr_) << "Delete period history from: " << start << " to " << end;
    db_->clearPeriodDataHistory(end);
  }
}

uint DagManager::setDagBlockOrder(blk_hash_t const &new_anchor, uint64_t period, vec_blk_t const &dag_order) {
  LOG(log_dg_) << "setDagBlockOrder called with anchor " << new_anchor << " and period " << period;
  if (period != period_ + 1) {
    LOG(log_er_) << " Inserting period (" << period << ") anchor " << new_anchor
                 << " does not match ..., previous internal period (" << period_ << ") " << anchor_;
    return 0;
  }

  if (new_anchor == NULL_BLOCK_HASH) {
    period_ = period;
    LOG(log_nf_) << "Set new period " << period << " with NULL_BLOCK_HASH anchor";
    return 0;
  }

  // Update dag counts correctly
  // When synced and gossiping there should not be anything to update
  // When syncing we must check if some of the DAG blocks are both in sync block and in memory DAG although
  // non-finalized block should be empty when syncing, maybe we should clear it if we are deep out of sync to improve
  // performance
  std::unordered_set<blk_hash_t> non_finalized_blocks_set;
  for (auto const &level : non_finalized_blks_) {
    for (auto const &blk : level.second) {
      non_finalized_blocks_set.insert(blk);
    }
  }
  // Only update counter for blocks that are in the dag_order and not in memory DAG, this is only possible when pbft
  // syncing and processing sync block
  std::vector<DagBlock> dag_blocks_to_update_counters;
  for (auto const &blk : dag_order) {
    if (non_finalized_blocks_set.count(blk) == 0) {
      auto dag_block = dag_blk_mgr_->getDagBlock(blk);
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
  addToDag(new_anchor, blk_hash_t(), vec_blk_t(), 0, true);

  const auto anchor_block_level = dag_blk_mgr_->getDagBlock(new_anchor)->getLevel();
  if (anchor_block_level > dag_expiry_limit_) {
    dag_blk_mgr_->setDagExpiryLevel(anchor_block_level - dag_expiry_limit_);
  }

  std::unordered_map<blk_hash_t, std::shared_ptr<DagBlock>> expired_dag_blocks_to_remove;
  std::vector<trx_hash_t> expired_dag_blocks_transactions;

  for (auto &v : non_finalized_blocks) {
    for (auto &blk_hash : v.second) {
      if (dag_order_set.count(blk_hash) != 0) {
        continue;
      }

      auto dag_block = dag_blk_mgr_->getDagBlock(blk_hash);
      auto pivot_hash = dag_block->getPivot();

      if (validateBlockNotExpired(dag_block, expired_dag_blocks_to_remove)) {
        addToDag(blk_hash, pivot_hash, dag_block->getTips(), dag_block->getLevel(), false);
      } else {
        db_->removeDagBlock(blk_hash);
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
      auto dag_block = dag_blk_mgr_->getDagBlock(blk);
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

  const auto dag_expiry_level = dag_blk_mgr_->getDagExpiryLevel();
  if (block_points_to_expired_block || dag_block->getLevel() < dag_expiry_level) {
    LOG(log_nf_) << "Dropping expired block in setDagBlockOrder: " << blk_hash << ". Expiry level: " << dag_expiry_level
                 << ". Block level: " << dag_block->getLevel();
    expired_dag_blocks_to_remove[blk_hash] = dag_block;
    assert(blk_hash != frontier_.pivot);
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
        addToDag(anchor_, blk_hash_t(), vec_blk_t(), 0, true);
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
        // Verify VDF solution
        try {
          blk.verifyVdf(dag_blk_mgr_->sortitionParamsManager().getSortitionParams(*propose_period),
                        db_->getPeriodBlockHash(*propose_period));
        } catch (vdf_sortition::VdfSortition::InvalidVdfSortition const &e) {
          LOG(log_er_) << "DAG block " << blk.getHash() << " with " << blk.getLevel()
                       << " level failed on VDF verification with pivot hash " << blk.getPivot() << " reason "
                       << e.what();
          break;
        }
      }

      addDagBlock(std::move(blk), {}, false, false);
    }
  }
  trx_mgr_->recoverNonfinalizedTransactions();
}

const std::pair<uint64_t, std::map<uint64_t, std::unordered_set<blk_hash_t>>> DagManager::getNonFinalizedBlocks()
    const {
  SharedLock lock(mutex_);
  return {period_, non_finalized_blks_};
}

const std::tuple<uint64_t, std::vector<std::shared_ptr<DagBlock>>, SharedTransactions>
DagManager::getNonFinalizedBlocksWithTransactions(const std::unordered_set<blk_hash_t> &known_hashes) const {
  SharedLock lock(mutex_);
  std::vector<std::shared_ptr<DagBlock>> dag_blocks;
  std::unordered_set<trx_hash_t> unique_trxs;
  std::vector<trx_hash_t> trx_to_query;
  for (const auto &level_blocks : non_finalized_blks_) {
    for (const auto &hash : level_blocks.second) {
      if (known_hashes.count(hash) == 0) {
        if (auto blk = dag_blk_mgr_->getDagBlock(hash); blk) {
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

}  // namespace taraxa
