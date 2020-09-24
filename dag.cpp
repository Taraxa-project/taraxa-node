#include "dag.hpp"

#include <algorithm>
#include <fstream>
#include <queue>
#include <stack>
#include <tuple>
#include <unordered_set>
#include <utility>
#include <vector>

#include "transaction_manager.hpp"

namespace taraxa {

Dag::Dag(std::string const &genesis, addr_t node_addr) {
  LOG_OBJECTS_CREATE("DAGMGR");
  vertex_hash pivot = "";
  std::vector<vertex_hash> tips;
  // add genesis block
  addVEEs(genesis, pivot, tips);
}

uint64_t Dag::getNumVertices() const {
  sharedLock lock(mutex_);
  return boost::num_vertices(graph_);
}
uint64_t Dag::getNumEdges() const {
  sharedLock lock(mutex_);
  return boost::num_edges(graph_);
}

bool Dag::hasVertex(vertex_hash const &v) const {
  sharedLock lock(mutex_);
  return graph_.vertex(v) != graph_.null_vertex();
}

void Dag::getLeaves(std::vector<vertex_hash> &tips) const {
  sharedLock lock(mutex_);
  vertex_index_map_const_t index_map = boost::get(boost::vertex_index, graph_);
  std::vector<vertex_t> leaves;
  collectLeafVertices(leaves);
  std::transform(leaves.begin(), leaves.end(), std::back_inserter(tips),
                 [index_map](const vertex_t &leaf) { return index_map[leaf]; });
}

bool Dag::addVEEs(vertex_hash const &new_vertex, vertex_hash const &pivot,
                  std::vector<vertex_hash> const &tips) {
  uLock lock(mutex_);
  assert(!new_vertex.empty());

  // add vertex
  auto now(std::chrono::system_clock::now());
  vertex_t ret = add_vertex(new_vertex, graph_);
  boost::get(boost::vertex_index, graph_)[ret] = new_vertex;
  edge_index_map_t weight_map = boost::get(boost::edge_index, graph_);

  edge_t edge;
  bool res = true;

  // Note: add edges,
  // *** important
  // Add a new block, edges are pointing from pivot to new_veretx
  if (!pivot.empty()) {
    std::tie(edge, res) = boost::add_edge_by_label(pivot, new_vertex, graph_);
    weight_map[edge] = 1;
    if (!res) {
      LOG(log_wr_) << "Creating pivot edge \n"
                   << pivot << "\n-->\n"
                   << new_vertex << " \nunsuccessful!" << std::endl;
    }
  }
  bool res2 = true;
  for (auto const &e : tips) {
    std::tie(edge, res2) = boost::add_edge_by_label(e, new_vertex, graph_);
    weight_map[edge] = 0;

    if (!res2) {
      LOG(log_wr_) << "Creating tip edge \n"
                   << e << "\n-->\n"
                   << new_vertex << " \nunsuccessful!" << std::endl;
    }
  }
  res &= res2;
  return res;
}

void Dag::drawGraph(std::string filename) const {
  sharedLock lock(mutex_);
  std::ofstream outfile(filename.c_str());
  auto index_map = boost::get(boost::vertex_index, graph_);
  auto weight_map = boost::get(boost::edge_index, graph_);

  boost::write_graphviz(outfile, graph_, vertex_label_writer(index_map),
                        edge_label_writer(weight_map));
  std::cout << "Dot file " << filename << " generated!" << std::endl;
  std::cout << "Use \"dot -Tpdf <dot file> -o <pdf file>\" to generate pdf file"
            << std::endl;
}

Dag::vertex_t Dag::addVertex(vertex_hash const &hash) {
  vertex_t ret = add_vertex(hash, graph_);
  vertex_index_map_t index_map = boost::get(boost::vertex_index, graph_);
  index_map[ret] = hash;
  return ret;
}

void Dag::delVertex(vertex_hash const &hash) {
  upgradableLock lock(mutex_);
  vertex_t to_be_removed = graph_.vertex(hash);
  if (to_be_removed == graph_.null_vertex()) {
    return;
  }

  upgradeLock ll(lock);
  clear_vertex_by_label(hash, graph_);
  remove_vertex(hash, graph_);
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

void Dag::getEpFriendVertices(vertex_hash const &from, vertex_hash const &to,
                              std::vector<vertex_hash> &epfriend) const {
  sharedLock lock(mutex_);

  vertex_t source = graph_.vertex(from);
  vertex_t target = graph_.vertex(to);

  if (source == graph_.null_vertex()) {
    LOG(log_wr_) << "Cannot find vertex (from) (getEpFriendVertices) " << from
                 << "\n";
    return;
  }
  if (target == graph_.null_vertex()) {
    LOG(log_wr_) << "Cannot find vertex (to) " << to << "\n";
    return;
  }
  epfriend.clear();
  vertex_iter_t s, e;
  vertex_index_map_const_t index_map = boost::get(boost::vertex_index, graph_);

  // iterator all vertex
  for (std::tie(s, e) = boost::vertices(graph_); s != e; ++s) {
    if (*s == source) {
      continue;
    }
    if (!reachable(*s, source) && reachable(*s, target)) {
      epfriend.emplace_back(index_map[*s]);
    }
  }
}
// only iterate only through non finalized blocks
bool Dag::computeOrder(vertex_hash const &anchor,
                       std::vector<vertex_hash> &ordered_period_vertices) {
  uLock lock(mutex_);

  vertex_t target = graph_.vertex(anchor);

  if (target == graph_.null_vertex()) {
    LOG(log_wr_) << "Dag::ComputeOrder cannot find vertex (anchor) " << anchor
                 << "\n";
    return false;
  }
  ordered_period_vertices.clear();

  vertex_iter_t s, e;
  vertex_index_map_t index_map = boost::get(
      boost::vertex_index, graph_);         // from vertex_descriptor to hash
  std::map<blk_hash_t, vertex_t> epfriend;  // this is unordered epoch
  if (!findDagBlock(anchor)) {
    LOG(log_er_) << "Anchor is not in recent_added_blks " << anchor;
  }
  epfriend[blk_hash_t(index_map[target])] = target;

  // Step 1: collect all epoch blks that can reach anchor
  // Erase from recent_added_blks after mark epoch number if finialized

  upgradableLock blocks_lock(blocks_access_);
  auto iter = non_finalized_blks_.begin();
  while (iter != non_finalized_blks_.end()) {
    auto v = graph_.vertex(*iter);
    if (reachable(v, target)) {
      iter++;
      epfriend[blk_hash_t(index_map[v])] = v;
    } else {
      iter++;
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
      for (std::tie(adj_s, adj_e) = adjacenct_vertices(cur.first, graph_);
           adj_s != adj_e; adj_s++) {
        if (epfriend.find(blk_hash_t(index_map[*adj_s])) ==
            epfriend.end()) {  // not in this epoch
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

bool Dag::findDagBlock(vertex_hash const &block_hash) const {
  sharedLock lock(blocks_access_);
  return non_finalized_blks_.find(block_hash) != non_finalized_blks_.end();
}

std::unordered_set<std::string> Dag::getUnOrderedDagBlks() const {
  sharedLock lock(blocks_access_);
  return non_finalized_blks_;
}
void Dag::addNonFinalizedDagBlks(vertex_hash const &hash) {
  uLock lock(blocks_access_);
  non_finalized_blks_.insert(hash);
}

/**
 * Iterative version
 * Steps rounds
 * 1. post order traversal
 * 2. from leave, count weight and propagate up
 * 3. collect path
 */

void PivotTree::getGhostPath(vertex_hash const &vertex,
                             std::vector<vertex_hash> &pivot_chain) const {
  sharedLock lock(mutex_);
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
    vertex_t next;

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

DagManager::DagManager(std::string const &genesis, addr_t node_addr,
                       std::shared_ptr<TransactionManager> trx_mgr,
                       std::shared_ptr<PbftChain> pbft_chain,
                       std::shared_ptr<DbStorage> db) try
    : inserting_index_counter_(0),
      total_dag_(std::make_shared<Dag>(genesis, node_addr)),
      pivot_tree_(std::make_shared<PivotTree>(genesis, node_addr)),
      genesis_(genesis),
      trx_mgr_(trx_mgr),
      pbft_chain_(pbft_chain),
      db_(db),
      anchor_(genesis),
      period_(0) {
  LOG_OBJECTS_CREATE("DAGMGR");
  DagBlock blk;
  string pivot;
  std::vector<std::string> tips;
  getLatestPivotAndTips(pivot, tips);
  DagFrontier frontier;
  frontier.pivot = blk_hash_t(pivot);
  if (trx_mgr) {
    trx_mgr->updateNonce(blk, frontier);
  }
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
  trx_mgr_ = nullptr;
  pbft_chain_ = nullptr;
}

std::pair<uint64_t, uint64_t> DagManager::getNumVerticesInDag() const {
  sharedLock lock(mutex_);
  return {total_dag_->getNumVertices(), pivot_tree_->getNumVertices()};
}

std::pair<uint64_t, uint64_t> DagManager::getNumEdgesInDag() const {
  sharedLock lock(mutex_);
  return {total_dag_->getNumEdges(), pivot_tree_->getNumEdges()};
}

void DagManager::drawTotalGraph(std::string const &str) const {
  sharedLock lock(mutex_);
  total_dag_->drawGraph(str);
}

void DagManager::drawPivotGraph(std::string const &str) const {
  sharedLock lock(mutex_);
  pivot_tree_->drawGraph(str);
}

bool DagManager::dagHasVertex(blk_hash_t const &blk_hash) {
  if (total_dag_->hasVertex(blk_hash.toString())) {
    LOG(log_dg_) << "DAG Block " << blk_hash << " is in DAG already! ";
    return true;
  }
  return false;
}

bool DagManager::pivotAndTipsAvailable(DagBlock const &blk) {
  auto dag_blk_hash = blk.getHash();
  auto dag_blk_pivot = blk.getPivot();
  uLock lock(mutex_);

  if (db_->getDagBlock(dag_blk_pivot) == nullptr) {
    LOG(log_dg_) << "DAG Block " << dag_blk_hash << " pivot " << dag_blk_pivot
                 << " unavailable";
    return false;
  }

  for (auto const &t : blk.getTips()) {
    if (db_->getDagBlock(t) == nullptr) {
      LOG(log_dg_) << "DAG Block " << dag_blk_hash << " tip " << t
                   << " unavailable";
      return false;
    }
  }

  return true;
}

void DagManager::addDagBlock(DagBlock const &blk, bool finalized) {
  auto blk_hash = blk.getHash();
  auto blk_hash_str = blk_hash.toString();
  auto pivot_hash = blk.getPivot();
  DagFrontier frontier;
  uLock lock(mutex_);

  if (db_->getDagBlock(blk_hash) != nullptr) {
    LOG(log_dg_) << "DAG Block " << blk_hash << " is in DAG already! ";
    return;
  }

  std::vector<std::string> tips;
  for (auto const &t : blk.getTips()) {
    std::string tip = t.toString();
    tips.push_back(tip);
  }

  level_t current_max_level = getMaxLevel();
  max_level_ = std::max(current_max_level, blk.getLevel());

  addToDag(blk_hash_str, pivot_hash.toString(), tips, finalized);

  auto [p, ts] = getFrontier();
  frontier.pivot = blk_hash_t(p);
  for (auto const &t : ts) {
    frontier.tips.emplace_back(blk_hash_t(t));
  }
  if (trx_mgr_) {
    trx_mgr_->updateNonce(blk, frontier);
  }
  LOG(log_dg_) << " Update nonce table of blk " << blk.getHash() << "anchor "
               << anchor_ << " pivot = " << frontier.pivot
               << " tips: " << frontier.tips;
}

void DagManager::drawGraph(std::string const &dotfile) const {
  drawPivotGraph("pivot." + dotfile);
  drawTotalGraph("total." + dotfile);
}

void DagManager::addToDag(std::string const &hash, std::string const &pivot,
                          std::vector<std::string> const &tips,
                          bool finalized) {
  total_dag_->addVEEs(hash, pivot, tips);
  if (!finalized) {
    total_dag_->addNonFinalizedDagBlks(hash);
    pivot_tree_->addVEEs(hash, pivot, {});
  }
  LOG(log_dg_) << " Insert block to DAG : " << hash;
}

bool DagManager::getLatestPivotAndTips(std::string &pivot,
                                       std::vector<std::string> &tips) const {
  // make sure the state of dag is the same when collection pivot and tips
  sharedLock lock(mutex_);
  pivot.clear();
  tips.clear();
  std::tie(pivot, tips) = getFrontier();

  return !pivot.empty();
}

std::pair<std::string, std::vector<std::string>> DagManager::getFrontier()
    const {
  std::string pivot;
  std::vector<std::string> tips;
  std::vector<std::string> pivot_chain;

  auto last_pivot = anchor_;
  pivot_tree_->getGhostPath(last_pivot, pivot_chain);
  if (!pivot_chain.empty()) {
    pivot = pivot_chain.back();
    total_dag_->getLeaves(tips);
    // remove pivot from tips
    auto end =
        std::remove_if(tips.begin(), tips.end(),
                       [pivot](std::string const &s) { return s == pivot; });
    tips.erase(end, tips.end());
  }
  return {pivot, tips};
}

void DagManager::collectTotalLeaves(std::vector<std::string> &leaves) const {
  total_dag_->getLeaves(leaves);
}
void DagManager::getGhostPath(std::string const &source,
                              std::vector<std::string> &ghost) const {
  pivot_tree_->getGhostPath(source, ghost);
}

void DagManager::getGhostPath(std::vector<std::string> &ghost) const {
  auto last_pivot = anchor_;
  ghost.clear();
  pivot_tree_->getGhostPath(last_pivot, ghost);
}
std::vector<std::string> DagManager::getEpFriendBetweenPivots(
    std::string const &from, std::string const &to) {
  std::vector<std::string> epfriend;

  total_dag_->getEpFriendVertices(from, to, epfriend);
  return epfriend;
}

std::vector<std::string> DagManager::getPivotChain(
    std::string const &vertex) const {
  std::vector<std::string> ret;
  pivot_tree_->getGhostPath(vertex, ret);
  return ret;
}

// return {period, block order}, for pbft-pivot-blk proposing
std::pair<uint64_t, std::shared_ptr<vec_blk_t>> DagManager::getDagBlockOrder(
    blk_hash_t const &anchor) {
  LOG(log_dg_) << "getDagBlockOrder called with anchor " << anchor;
  vec_blk_t orders;
  auto period = getDagBlockOrder(anchor, orders);
  return {period, std::make_shared<vec_blk_t>(orders)};
}
// receive pbft-povit-blk, update periods
uint DagManager::setDagBlockOrder(blk_hash_t const &anchor, uint64_t period) {
  LOG(log_dg_) << "setDagBlockOrder called with anchor " << anchor
               << " and period " << period;
  auto res = setDagBlockPeriod(anchor, period);
  return res;
}

uint64_t DagManager::getDagBlockOrder(blk_hash_t const &anchor,
                                      vec_blk_t &orders) {
  // TODO: need to check if the anchor already processed
  // if the period already processed
  orders.clear();

  std::vector<std::string> blk_orders;

  // TODO: need to use the same pivot/tips that are stored in nonce map

  if (blk_hash_t(anchor_) == anchor) {
    LOG(log_wr_) << "Query period from " << blk_hash_t(anchor_) << " to "
                 << anchor << " not ok " << std::endl;
    return period_;
  }

  auto new_period = period_ + 1;

  auto ok = total_dag_->computeOrder(anchor.toString(), blk_orders);
  if (!ok) {
    LOG(log_er_) << " Create period " << new_period << " anchor: " << anchor
                 << " failed " << std::endl;
    return 0;
  }

  std::transform(blk_orders.begin(), blk_orders.end(),
                 std::back_inserter(orders),
                 [](const std::string &i) { return blk_hash_t(i); });
  LOG(log_dg_) << "Get period " << new_period << " from " << blk_hash_t(anchor_)
               << " to " << anchor << " with " << blk_orders.size() << " blks"
               << std::endl;
  return new_period;
}
uint DagManager::setDagBlockPeriod(blk_hash_t const &anchor, uint64_t period) {
  if (period != period_ + 1) {
    LOG(log_er_) << " Inserting period (" << period << ") anchor " << anchor
                 << " does not match ..., previous internal period (" << period_
                 << ") " << anchor_;
    return 0;
  }
  auto prev = anchor_;
  std::vector<std::string> blk_orders;

  std::vector<std::string> leaves;
  total_dag_->getLeaves(leaves);

  // Total DAG will only include leaves from the last period and non-finalized
  // blocks
  total_dag_->clear();
  for (auto &leave : leaves)
    addDagBlock(*db_->getDagBlock(blk_hash_t(leave)), true);

  // Pivot tree will only include anchor from the last period and non-finalized
  // blocks
  pivot_tree_->clear();
  pivot_tree_->addVEEs(
      anchor_, db_->getDagBlock(blk_hash_t(anchor_))->getPivot().toString(),
      {});

  old_anchor_ = anchor_;
  anchor_ = anchor.toString();

  LOG(log_nf_) << "Set new period " << period << " with anchor " << anchor;

  return blk_orders.size();
}

void DagManager::recoverAnchors(uint64_t pbft_chain_size) {
  /*std::vector<blk_hash_t> anchors;
  // Use index to save period, so index 0 is empty
  anchors.resize(pbft_chain_size + 1);

  blk_hash_t pbft_block_hash = pbft_chain_->getLastPbftBlockHash();
  PbftBlock pbft_block = pbft_chain_->getPbftBlockInChain(pbft_block_hash);
  blk_hash_t dag_block_hash_as_anchor = pbft_block.getPivotDagBlockHash();
  uint64_t period = pbft_block.getPeriod();
  anchors[period] = dag_block_hash_as_anchor;
  for (auto i = pbft_chain_size - 1; i > 0; --i) {
    pbft_block_hash = pbft_block.getPrevBlockHash();
    pbft_block = pbft_chain_->getPbftBlockInChain(pbft_block_hash);
    dag_block_hash_as_anchor = pbft_block.getPivotDagBlockHash();
    period = pbft_block.getPeriod();
    assert(i == period);
    anchors[period] = dag_block_hash_as_anchor;
  }
  uLock lock(anchors_access_);
  for (auto i = 1; i < anchors.size(); ++i) {
    anchors_.push_back({anchors[i].toString(), i});
  }*/
}

}  // namespace taraxa