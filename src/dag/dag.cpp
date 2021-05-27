#include "dag.hpp"

#include <algorithm>
#include <fstream>
#include <queue>
#include <stack>
#include <tuple>
#include <unordered_set>
#include <utility>
#include <vector>

#include "transaction_manager/transaction_manager.hpp"

namespace taraxa {

Dag::Dag(std::string const &genesis, addr_t node_addr) {
  LOG_OBJECTS_CREATE("DAGMGR");
  vertex_hash pivot = "";
  std::vector<vertex_hash> tips;
  // add genesis block
  addVEEs(genesis, pivot, tips);
}

uint64_t Dag::getNumVertices() const { return boost::num_vertices(graph_); }
uint64_t Dag::getNumEdges() const { return boost::num_edges(graph_); }

bool Dag::hasVertex(vertex_hash const &v) const { return graph_.vertex(v) != graph_.null_vertex(); }

void Dag::getLeaves(std::vector<vertex_hash> &tips) const {
  vertex_index_map_const_t index_map = boost::get(boost::vertex_index, graph_);
  std::vector<vertex_t> leaves;
  collectLeafVertices(leaves);
  std::transform(leaves.begin(), leaves.end(), std::back_inserter(tips),
                 [index_map](const vertex_t &leaf) { return index_map[leaf]; });
}

bool Dag::addVEEs(vertex_hash const &new_vertex, vertex_hash const &pivot, std::vector<vertex_hash> const &tips) {
  assert(!new_vertex.empty());

  // add vertex
  vertex_t ret = add_vertex(new_vertex, graph_);
  boost::get(boost::vertex_index, graph_)[ret] = new_vertex;
  edge_index_map_t weight_map = boost::get(boost::edge_index, graph_);

  edge_t edge;
  bool res = true;

  // Note: add edges,
  // *** important
  // Add a new block, edges are pointing from pivot to new_veretx
  if (!pivot.empty()) {
    if (hasVertex(pivot)) {
      std::tie(edge, res) = boost::add_edge_by_label(pivot, new_vertex, graph_);
      weight_map[edge] = 1;
      if (!res) {
        LOG(log_wr_) << "Creating pivot edge \n" << pivot << "\n-->\n" << new_vertex << " \nunsuccessful!" << std::endl;
      }
    }
  }
  bool res2 = true;
  for (auto const &e : tips) {
    if (hasVertex(e)) {
      std::tie(edge, res2) = boost::add_edge_by_label(e, new_vertex, graph_);
      weight_map[edge] = 0;

      if (!res2) {
        LOG(log_wr_) << "Creating tip edge \n" << e << "\n-->\n" << new_vertex << " \nunsuccessful!" << std::endl;
      }
    }
  }
  res &= res2;
  return res;
}

void Dag::drawGraph(std::string filename) const {
  std::ofstream outfile(filename.c_str());
  auto index_map = boost::get(boost::vertex_index, graph_);
  auto weight_map = boost::get(boost::edge_index, graph_);

  boost::write_graphviz(outfile, graph_, vertex_label_writer(index_map), edge_label_writer(weight_map));
  std::cout << "Dot file " << filename << " generated!" << std::endl;
  std::cout << "Use \"dot -Tpdf <dot file> -o <pdf file>\" to generate pdf file" << std::endl;
}

void Dag::clear() { graph_ = graph_t(); }

Dag::edge_t Dag::addEdge(Dag::vertex_t v1, Dag::vertex_t v2) {
  auto ret = add_edge(v1, v2, graph_);
  assert(ret.second);
  return ret.first;
}

Dag::edge_t Dag::addEdge(vertex_hash const &v1, vertex_hash const &v2) {
  assert(graph_.vertex(v1) != graph_.null_vertex());
  assert(graph_.vertex(v2) != graph_.null_vertex());
  // lock should be behind assert
  edge_t edge;
  bool res;
  std::tie(edge, res) = add_edge_by_label(v1, v2, graph_);
  assert(res);
  return edge;
}

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
bool Dag::computeOrder(vertex_hash const &anchor, std::vector<vertex_hash> &ordered_period_vertices,
                       std::map<uint64_t, std::vector<std::string>> const &non_finalized_blks) {
  vertex_t target = graph_.vertex(anchor);

  if (target == graph_.null_vertex()) {
    LOG(log_wr_) << "Dag::ComputeOrder cannot find vertex (anchor) " << anchor << "\n";
    return false;
  }
  ordered_period_vertices.clear();

  vertex_iter_t s, e;
  vertex_index_map_t index_map = boost::get(boost::vertex_index, graph_);  // from vertex_descriptor to hash
  std::map<blk_hash_t, vertex_t> epfriend;                                 // this is unordered epoch
  epfriend[blk_hash_t(index_map[target])] = target;

  // Step 1: collect all epoch blks that can reach anchor
  // Erase from recent_added_blks after mark epoch number if finialized

  for (auto &l : non_finalized_blks) {
    for (auto &blk : l.second) {
      auto v = graph_.vertex(blk);
      if (reachable(v, target)) {
        epfriend[blk_hash_t(index_map[v])] = v;
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
        if (epfriend.find(blk_hash_t(index_map[*adj_s])) == epfriend.end()) {  // not in this epoch
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

void PivotTree::getGhostPath(vertex_hash const &vertex, std::vector<vertex_hash> &pivot_chain) const {
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

DagManager::DagManager(std::string const &genesis, addr_t node_addr, std::shared_ptr<TransactionManager> trx_mgr,
                       std::shared_ptr<PbftChain> pbft_chain, std::shared_ptr<DbStorage> db) try
    : pivot_tree_(std::make_shared<PivotTree>(genesis, node_addr)),
      total_dag_(std::make_shared<Dag>(genesis, node_addr)),
      trx_mgr_(trx_mgr),
      pbft_chain_(pbft_chain),
      db_(db),
      anchor_(genesis),
      period_(0),
      genesis_(genesis) {
  LOG_OBJECTS_CREATE("DAGMGR");
  DagBlock blk;
  string pivot;
  std::vector<std::string> tips;
  getLatestPivotAndTips(pivot, tips);
  frontier_.pivot = blk_hash_t(pivot);
  for (auto const &t : tips) {
    frontier_.tips.emplace_back(blk_hash_t(t));
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
  unique_lock lock(mutex_);
  trx_mgr_ = nullptr;
}

std::pair<uint64_t, uint64_t> DagManager::getNumVerticesInDag() const {
  sharedLock lock(mutex_);
  return {db_->getNumDagBlocks(), total_dag_->getNumVertices()};
}

std::pair<uint64_t, uint64_t> DagManager::getNumEdgesInDag() const {
  sharedLock lock(mutex_);
  return {db_->getDagEdgeCount(), total_dag_->getNumEdges()};
}

void DagManager::drawTotalGraph(std::string const &str) const {
  sharedLock lock(mutex_);
  total_dag_->drawGraph(str);
}

void DagManager::drawPivotGraph(std::string const &str) const {
  sharedLock lock(mutex_);
  pivot_tree_->drawGraph(str);
}

bool DagManager::pivotAndTipsAvailable(DagBlock const &blk) {
  auto dag_blk_hash = blk.getHash();
  auto dag_blk_pivot = blk.getPivot();

  if (db_->getDagBlock(dag_blk_pivot) == nullptr) {
    LOG(log_dg_) << "DAG Block " << dag_blk_hash << " pivot " << dag_blk_pivot << " unavailable";
    return false;
  }

  for (auto const &t : blk.getTips()) {
    if (db_->getDagBlock(t) == nullptr) {
      LOG(log_dg_) << "DAG Block " << dag_blk_hash << " tip " << t << " unavailable";
      return false;
    }
  }

  return true;
}

DagFrontier DagManager::getDagFrontier() {
  sharedLock lock(mutex_);
  return frontier_;
}

void DagManager::addDagBlock(DagBlock const &blk, bool finalized, bool save) {
  auto write_batch = db_->createWriteBatch();
  {
    uLock lock(mutex_);
    if (save) {
      db_->saveDagBlock(blk, write_batch);
    }
    auto blk_hash = blk.getHash();
    auto blk_hash_str = blk_hash.toString();
    auto pivot_hash = blk.getPivot();

    std::vector<std::string> tips;
    for (auto const &t : blk.getTips()) {
      std::string tip = t.toString();
      tips.push_back(tip);
    }

    level_t current_max_level = max_level_;
    max_level_ = std::max(current_max_level, blk.getLevel());

    addToDag(blk_hash_str, pivot_hash.toString(), tips, blk.getLevel(), write_batch, finalized);

    auto [p, ts] = getFrontier();
    frontier_.pivot = blk_hash_t(p);
    frontier_.tips.clear();
    for (auto const &t : ts) {
      frontier_.tips.emplace_back(blk_hash_t(t));
    }
    db_->commitWriteBatch(write_batch);
  }
  LOG(log_dg_) << " Update frontier after adding block " << blk.getHash() << "anchor " << anchor_
               << " pivot = " << frontier_.pivot << " tips: " << frontier_.tips;
}

void DagManager::drawGraph(std::string const &dotfile) const {
  sharedLock lock(mutex_);
  drawPivotGraph("pivot." + dotfile);
  drawTotalGraph("total." + dotfile);
}

void DagManager::addToDag(std::string const &hash, std::string const &pivot, std::vector<std::string> const &tips,
                          uint64_t level, const taraxa::DbStorage::BatchPtr &write_batch, bool finalized) {
  total_dag_->addVEEs(hash, pivot, tips);
  pivot_tree_->addVEEs(hash, pivot, {});
  db_->addDagBlockStateToBatch(write_batch, blk_hash_t(hash), finalized);
  if (finalized) {
    finalized_blks_[level].push_back(hash);
  } else {
    non_finalized_blks_[level].push_back(hash);
  }
  LOG(log_dg_) << " Insert block to DAG : " << hash;
}

bool DagManager::getLatestPivotAndTips(std::string &pivot, std::vector<std::string> &tips) const {
  // make sure the state of dag is the same when collection pivot and tips
  sharedLock lock(mutex_);
  pivot.clear();
  tips.clear();
  std::tie(pivot, tips) = getFrontier();

  return !pivot.empty();
}

std::pair<std::string, std::vector<std::string>> DagManager::getFrontier() const {
  std::string pivot;
  std::vector<std::string> tips;
  std::vector<std::string> pivot_chain;

  auto last_pivot = anchor_;
  pivot_tree_->getGhostPath(last_pivot, pivot_chain);
  if (!pivot_chain.empty()) {
    pivot = pivot_chain.back();
    total_dag_->getLeaves(tips);
    // remove pivot from tips
    auto end = std::remove_if(tips.begin(), tips.end(), [pivot](std::string const &s) { return s == pivot; });
    tips.erase(end, tips.end());
  }
  return {pivot, tips};
}

void DagManager::collectTotalLeaves(std::vector<std::string> &leaves) const {
  sharedLock lock(mutex_);
  total_dag_->getLeaves(leaves);
}
void DagManager::getGhostPath(std::string const &source, std::vector<std::string> &ghost) const {
  sharedLock lock(mutex_);
  pivot_tree_->getGhostPath(source, ghost);
}

void DagManager::getGhostPath(std::vector<std::string> &ghost) const {
  sharedLock lock(mutex_);
  auto last_pivot = anchor_;
  ghost.clear();
  pivot_tree_->getGhostPath(last_pivot, ghost);
}

// return {period, block order}, for pbft-pivot-blk proposing
std::pair<uint64_t, std::shared_ptr<vec_blk_t>> DagManager::getDagBlockOrder(blk_hash_t const &anchor) {
  sharedLock lock(mutex_);
  // TODO: need to check if the anchor already processed
  // if the period already processed
  vec_blk_t orders;

  std::vector<std::string> blk_orders;

  if (blk_hash_t(anchor_) == anchor) {
    LOG(log_wr_) << "Query period from " << blk_hash_t(anchor_) << " to " << anchor << " not ok " << std::endl;
    return {0, std::make_shared<vec_blk_t>(orders)};
  }

  auto new_period = period_ + 1;

  auto ok = total_dag_->computeOrder(anchor.toString(), blk_orders, non_finalized_blks_);
  if (!ok) {
    LOG(log_er_) << " Create period " << new_period << " anchor: " << anchor << " failed " << std::endl;
    return {0, std::make_shared<vec_blk_t>(orders)};
  }

  std::transform(blk_orders.begin(), blk_orders.end(), std::back_inserter(orders),
                 [](const std::string &i) { return blk_hash_t(i); });
  LOG(log_dg_) << "Get period " << new_period << " from " << blk_hash_t(anchor_) << " to " << anchor << " with "
               << blk_orders.size() << " blks" << std::endl;

  return {new_period, std::make_shared<vec_blk_t>(orders)};
}

uint DagManager::setDagBlockOrder(blk_hash_t const &new_anchor, uint64_t period, vec_blk_t const &dag_order,
                                  const taraxa::DbStorage::BatchPtr &write_batch) {
  // TODO this function smells. It tries to manage in-memory and persistent state at the same time, which it
  // clearly lacks scope for. Generally, it's very sensitive to how it's called.
  // Also, it's clearly used only in conjunction with getDagBlockOrder - makes sense to merge these two.
  uLock lock(mutex_);
  LOG(log_dg_) << "setDagBlockOrder called with anchor " << new_anchor << " and period " << period;
  db_->putFinalizedDagBlockHashesByAnchor(*write_batch, new_anchor, dag_order);
  if (period != period_ + 1) {
    LOG(log_er_) << " Inserting period (" << period << ") anchor " << new_anchor
                 << " does not match ..., previous internal period (" << period_ << ") " << anchor_;
    return 0;
  }

  auto prev = anchor_;

  std::vector<std::string> leaves;
  total_dag_->getLeaves(leaves);
  std::set<std::string> leavesSet(leaves.begin(), leaves.end());

  total_dag_->clear();
  pivot_tree_->clear();
  auto finalized_blocks = finalized_blks_;
  auto non_finalized_blocks = non_finalized_blks_;
  finalized_blks_.clear();
  non_finalized_blks_.clear();

  // Total DAG will only include leaves from the last period and non-finalized
  // blocks
  // Pivot tree will only include anchor from the last period and non-finalized
  // blocks
  for (auto &v : finalized_blocks) {
    for (auto &blk : v.second) {
      auto block = db_->getDagBlock(blk_hash_t(blk));
      auto pivot_hash = block->getPivot();
      std::vector<std::string> tips;
      for (auto const &t : block->getTips()) {
        std::string tip = t.toString();
        tips.push_back(tip);
      }

      // Do not remove from total dag if a block is a leaf -- THERE IS A CHANCE
      // THAT THIS MIGHT NOT BE POSSIBLE SO MAYBE AN ASSERT WOULD BE BETTER
      if (leavesSet.count(blk) > 0) {
        addToDag(blk, pivot_hash.toString(), tips, block->getLevel(), write_batch, true);
      } else {
        db_->removeDagBlockStateToBatch(write_batch, blk_hash_t(blk));
      }
    }
  }

  bool new_anchor_found = false;
  for (auto &block : dag_order) {
    // Remove all just finalized except the leaves
    auto blk = block.toString();
    auto dag_block = db_->getDagBlock(block);
    auto pivot_hash = dag_block->getPivot();
    std::vector<std::string> tips;
    for (auto const &t : dag_block->getTips()) {
      std::string tip = t.toString();
      tips.push_back(tip);
    }
    // Verify anchor is included
    if (blk == new_anchor.toString()) {
      new_anchor_found = true;
    }

    if (leavesSet.count(blk) > 0 || blk == new_anchor.toString()) {
      addToDag(blk, pivot_hash.toString(), tips, dag_block->getLevel(), write_batch, true);
      db_->addDagBlockStateToBatch(write_batch, blk_hash_t(blk), true);
    } else {
      db_->removeDagBlockStateToBatch(write_batch, blk_hash_t(blk));
    }
  }
  assert(new_anchor_found);

  // Add remaining blocks that are not finalized
  std::set<blk_hash_t> dag_order_set(dag_order.begin(), dag_order.end());
  for (auto &v : non_finalized_blocks) {
    for (auto &blk : v.second) {
      if (dag_order_set.count(blk_hash_t(blk)) == 0) {
        auto dag_block = db_->getDagBlock(blk_hash_t(blk));
        auto pivot_hash = dag_block->getPivot();
        std::vector<std::string> tips;
        for (auto const &t : dag_block->getTips()) {
          std::string tip = t.toString();
          tips.push_back(tip);
        }
        addToDag(blk, pivot_hash.toString(), tips, dag_block->getLevel(), write_batch, false);
      }
    }
  }

  old_anchor_ = anchor_;
  anchor_ = new_anchor.toString();
  period_ = period;

  LOG(log_nf_) << "Set new period " << period << " with anchor " << new_anchor;

  return dag_order_set.size();
}

void DagManager::recoverDag() {
  if (pbft_chain_) {
    blk_hash_t pbft_block_hash = pbft_chain_->getLastPbftBlockHash();
    if (pbft_block_hash) {
      PbftBlock pbft_block = pbft_chain_->getPbftBlockInChain(pbft_block_hash);
      blk_hash_t dag_block_hash_as_anchor = pbft_block.getPivotDagBlockHash();
      period_ = pbft_block.getPeriod();
      anchor_ = dag_block_hash_as_anchor.toString();
      LOG(log_nf_) << "Recover anchor " << anchor_;

      pbft_block_hash = pbft_block.getPrevBlockHash();
      if (pbft_block_hash) {
        pbft_block = pbft_chain_->getPbftBlockInChain(pbft_block_hash);
        dag_block_hash_as_anchor = pbft_block.getPivotDagBlockHash();
        old_anchor_ = dag_block_hash_as_anchor.toString();
      }
    }
  }

  auto dag_state_map = db_->getAllDagBlockState();
  std::map<uint64_t, std::vector<blk_hash_t>> finalized, non_finalized;
  for (auto &it : dag_state_map) {
    if (it.second) {
      finalized[db_->getDagBlock(it.first)->getLevel()].push_back(it.first);
    } else {
      non_finalized[db_->getDagBlock(it.first)->getLevel()].push_back(it.first);
    }
  }
  for (auto &it : finalized) {
    for (auto &blk_hash : it.second) {
      auto blk = db_->getDagBlock(blk_hash);
      addDagBlock(*blk, true, false);
    }
  }
  for (auto &it : non_finalized) {
    for (auto &blk_hash : it.second) {
      auto blk = db_->getDagBlock(blk_hash);
      addDagBlock(*blk, false, false);
    }
  }
}

std::map<uint64_t, std::vector<std::string>> DagManager::getNonFinalizedBlocks() const {
  sharedLock lock(mutex_);
  return non_finalized_blks_;
}

}  // namespace taraxa