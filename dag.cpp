#include "dag.hpp"
#include <algorithm>
#include <fstream>
#include <queue>
#include <stack>
#include <tuple>
#include <unordered_set>
#include <utility>
#include <vector>

namespace taraxa {

Dag::Dag(std::string const &genesis) {
  vertex_hash pivot = "";
  std::vector<vertex_hash> tips;
  // add genesis block
  addVEEs(genesis, pivot, tips);
}
Dag::~Dag() {}

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
  for (auto const &leaf : leaves) {
    tips.emplace_back(index_map[leaf]);
  }
}

bool Dag::addVEEs(vertex_hash const &new_vertex, vertex_hash const &pivot,
                  std::vector<vertex_hash> const &tips) {
  uLock lock(mutex_);
  assert(!new_vertex.empty());

  // add vertex
  auto now(std::chrono::system_clock::now());
  vertex_t ret = add_vertex(new_vertex, graph_);
  vertex_index_map_t index_map = boost::get(boost::vertex_index, graph_);
  index_map[ret] = new_vertex;
  vertex_period_map_t epc_map = boost::get(boost::vertex_index1, graph_);
  epc_map[ret] = 0;  // means not finalized
  // <<std::endl;
  edge_t edge;
  bool res = true;

  // Note: add edges,
  // *** important
  // Add a new block, edges are pointing from pivot to new_veretx
  if (!pivot.empty()) {
    std::tie(edge, res) = boost::add_edge_by_label(pivot, new_vertex, graph_);
    if (!res) {
      LOG(log_wr_) << "Creating pivot edge \n"
                   << pivot << "\n-->\n"
                   << new_vertex << " \nunsuccessful!" << std::endl;
    }
  }
  bool res2 = true;
  for (auto const &e : tips) {
    std::tie(edge, res2) = boost::add_edge_by_label(e, new_vertex, graph_);
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
  auto ep_map = boost::get(boost::vertex_index1, graph_);

  boost::write_graphviz(outfile, graph_, label_writer(index_map, ep_map));
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
  vertex_t to_be_removed = graph_.vertex(hash);
  if (to_be_removed == graph_.null_vertex()) {
    return;
  }
  clear_vertex_by_label(hash, graph_);
  remove_vertex(hash, graph_);
}

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
bool Dag::computeOrder(bool finialized, vertex_hash const &anchor,
                       uint64_t ith_period,
                       std::unordered_set<vertex_hash> &recent_added_blks,
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
  vertex_index_map_t index_map = boost::get(boost::vertex_index, graph_);
  vertex_period_map_t ep_map = boost::get(boost::vertex_index1, graph_);
  std::map<blk_hash_t, vertex_t> epfriend;  // this is unordered epoch
  epfriend[blk_hash_t(index_map[target])] = target;

  // Step 1: collect all epoch blks that can reach anchor
  // Erase from recent_added_blks after mark epoch number if finialized

  auto iter = recent_added_blks.begin();
  while (iter != recent_added_blks.end()) {
    auto v = graph_.vertex(*iter);
    if (ep_map[v] > 0) {
      iter++;
      LOG(log_er_) << "The vertex " << index_map[v]
                   << " has been included in other period " << ep_map[v]
                   << std::endl;
      assert(0);
      return false;
    }
    if (reachable(v, target)) {
      if (finialized) {
        ep_map[v] = ith_period;
        // update periods table
        periods_[ith_period].insert(index_map[v]);
        iter = recent_added_blks.erase(iter);
      } else {
        iter++;
      }
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

void Dag::setVertexPeriod(vertex_hash const &vertex, uint64_t period) {
  vertex_t current = graph_.vertex(vertex);
  if (current == graph_.null_vertex()) {
    LOG(log_wr_) << "Cannot find vertex (setVertexPeriod) " << vertex
                 << " to set period\n";
    return;
  }
  auto ep = boost::get(boost::vertex_index1, graph_);
  ep[current] = period;
}

uint64_t Dag::getVertexPeriod(vertex_hash const &vertex) const {
  vertex_t current = graph_.vertex(vertex);
  if (current == graph_.null_vertex()) {
    LOG(log_wr_) << "Cannot find vertex (getVertexPeriod) " << vertex
                 << " to get period\n";
    return 0;
  }
  auto ep = boost::get(boost::vertex_index1, graph_);
  return ep[current];
}

void Dag::deletePeriod(uint64_t period) {
  if (period == 0) {
    return;
  }
  if (periods_.count(period) == 0) {
    LOG(log_wr_) << "Period " << period << " is empty ...";
    return;
  }
  auto vertices = periods_[period];
  for (auto const &v : vertices) {
    delVertex(v);
    periods_.erase(period);
  }
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

DagManager::DagManager(std::string const &genesis) try
    : dag_updated_(false),
      inserting_index_counter_(0),
      total_dag_(std::make_shared<Dag>(genesis)),
      pivot_tree_(std::make_shared<PivotTree>(genesis)),
      anchors_({genesis}),
      genesis_(genesis) {
} catch (std::exception &e) {
  std::cerr << e.what() << std::endl;
}

DagManager::~DagManager() {
  if (!stopped_) {
    stop();
  }
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
  sharedLock lock(mutex_);
  return {total_dag_->getNumVertices(), pivot_tree_->getNumVertices()};
}

std::pair<uint64_t, uint64_t> DagManager::getNumEdgesInDag() const {
  sharedLock lock(mutex_);
  return {total_dag_->getNumEdges(), pivot_tree_->getNumEdges()};
}
size_t DagManager::getBufferSize() const {
  sharedLock lock(mutex_);
  return sb_buffer_.size();
}

void DagManager::drawTotalGraph(std::string const &str) const {
  sharedLock lock(mutex_);
  total_dag_->drawGraph(str);
}

void DagManager::drawPivotGraph(std::string const &str) const {
  sharedLock lock(mutex_);
  pivot_tree_->drawGraph(str);
}

void DagManager::start() {
  if (!stopped_) return;
  stopped_ = false;
  sb_buffer_processing_thread_ =
      std::make_shared<boost::thread>(boost::thread([this]() { consume(); }));
}
void DagManager::stop() {
  if (stopped_) return;
  stopped_ = true;
  sb_buffer_condition.notify_all();
  sb_buffer_processing_thread_->join();
}
void DagManager::addDagBlock(DagBlock const &blk) {
  if (!addDagBlockInternal(blk)) {
    addToDagBuffer(blk);
  } else {
    stdLock lock(sb_bufer_mutex_);
    sb_buffer_condition.notify_one();
  }
}

bool DagManager::addDagBlockInternal(DagBlock const &blk) {
  uLock lock(mutex_);
  auto hash = blk.getHash().toString();
  auto h = blk.getHash();
  auto p = blk.getPivot();
  if (total_dag_->hasVertex(hash)) {
    LOG(log_dg_) << "Block is in DAG already! " << h << std::endl;
    return true;
  }

  std::string pivot = blk.getPivot().toString();
  if (!total_dag_->hasVertex(pivot)) {
    LOG(log_dg_) << "Block " << h << " pivot " << p << " unavailable"
                 << std::endl;
    return false;
  }

  std::vector<std::string> tips;
  for (auto const &t : blk.getTips()) {
    std::string tip = t.toString();
    if (!total_dag_->hasVertex(tip)) {
      LOG(log_dg_) << "Block " << h << " tip " << t << " unavailable"
                   << std::endl;
      return false;
    }
    tips.push_back(tip);
  }

  addToDag(hash, pivot, tips);
  LOG(log_dg_) << "Block " << h << " added to DAG";
  max_level_ = std::max(max_level_, blk.getLevel());
  recent_added_blks_.insert(hash);
  return true;
}

void DagManager::addToDagBuffer(DagBlock const &blk) {
  stdLock lock(sb_bufer_mutex_);
  sb_buffer_.push_back(std::make_shared<DagBlock>(blk));
  sb_buffer_condition.notify_one();
}

void DagManager::addToDag(std::string const &hash, std::string const &pivot,
                          std::vector<std::string> const &tips) {
  total_dag_->addVEEs(hash, pivot, tips);
  pivot_tree_->addVEEs(hash, pivot, {});
  LOG(log_nf_) << "Insert block to DAG : " << hash;
}

void DagManager::consume() {
  stdLock lock(sb_bufer_mutex_);
  while (!stopped_) {
    bool blockAdded = false;
    auto iter = sb_buffer_.begin();
    while (iter != sb_buffer_.end()) {
      if (stopped_) break;
      if (addDagBlockInternal(**iter)) {
        blockAdded = true;
        iter = sb_buffer_.erase(iter++);
      } else {
        ++iter;
      }
    }
    if (!blockAdded) {
      sb_buffer_condition.wait(lock);
    }
  }
}
bool DagManager::getLatestPivotAndTips(std::string &pivot,
                                       std::vector<std::string> &tips) const {
  bool ret = false;
  // make sure the state of dag is the same when collection pivot and tips
  sharedLock lock(mutex_);
  std::vector<std::string> pivot_chain;
  pivot.clear();
  tips.clear();
  pivot_tree_->getGhostPath(genesis_, pivot_chain);
  if (!pivot_chain.empty()) {
    pivot = pivot_chain.back();
    total_dag_->getLeaves(tips);
    // remove pivot from tips
    auto end =
        std::remove_if(tips.begin(), tips.end(),
                       [pivot](std::string const &s) { return s == pivot; });
    tips.erase(end, tips.end());
    ret = true;
  }
  return ret;
}

void DagManager::collectTotalLeaves(std::vector<std::string> &leaves) const {
  total_dag_->getLeaves(leaves);
}
void DagManager::getGhostPath(std::string const &source,
                              std::vector<std::string> &ghost) const {
  pivot_tree_->getGhostPath(source, ghost);
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

uint64_t DagManager::getDagBlockOrder(blk_hash_t const &anchor,
                                      vec_blk_t &orders) {
  // TODO: need to check if the anchor already processed
  // if the period already processed
  orders.clear();

  std::vector<std::string> blk_orders;
  assert(anchors_.size());
  auto prev = anchors_.back();

  if (blk_hash_t(prev) == anchor) {
    LOG(log_wr_) << "Query period from " << blk_hash_t(prev) << " to " << anchor
                 << " not ok " << std::endl;
    return anchors_.size() - 1;
  }

  auto new_period = anchors_.size();

  auto ok =
      total_dag_->computeOrder(false /* finalized */, anchor.toString(),
                               new_period, recent_added_blks_, blk_orders);
  if (!ok) {
    LOG(log_er_) << "Create period " << new_period << " anchor: " << anchor
                 << " failed " << std::endl;
    return 0;
  }

  for (auto const &i : blk_orders) {
    orders.emplace_back(blk_hash_t(i));
  }
  LOG(log_dg_) << "Get period " << new_period << " from " << blk_hash_t(prev)
               << " to " << anchor << " with " << blk_orders.size() << " blks"
               << std::endl;
  return new_period;
}
uint DagManager::setDagBlockPeriod(blk_hash_t const &anchor, uint64_t period) {
  if (period != anchors_.size()) {
    LOG(log_er_) << "Inserting period (" << period << ") anchor " << anchor
                 << " does not match ..., previous internal period ("
                 << anchors_.size() - 1 << ") " << anchors_.back();
    return 0;
  }
  auto prev = anchors_.back();
  std::vector<std::string> blk_orders;

  auto ok = total_dag_->computeOrder(true /* finalized */, anchor.toString(),
                                     period, recent_added_blks_, blk_orders);
  anchors_.emplace_back(anchor.toString());

  if (!ok) {
    LOG(log_er_) << "Create epoch " << period << " from " << blk_hash_t(prev)
                 << " to " << anchor << " failed ";
    return 0;
  }
  LOG(log_nf_) << "Set new period " << period << " with anchor " << anchor;
  return blk_orders.size();
}
}  // namespace taraxa