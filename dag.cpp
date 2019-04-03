/*
 * @Copyright: Taraxa.io
 * @Author: Chia-Chun Lin
 * @Date: 2018-12-14 10:59:17
 * @Last Modified by: Chia-Chun Lin
 * @Last Modified time: 2019-03-16 23:46:47
 */

#include <fstream>
#include <queue>
#include <stack>
#include <tuple>
#include <unordered_set>
#include <vector>

#include "dag.hpp"

namespace taraxa {

std::string const Dag::GENESIS =
    "0000000000000000000000000000000000000000000000000000000000000000";

Dag::Dag() : debug_(false), verbose_(false) {
  vertex_hash pivot = "";
  std::vector<vertex_hash> tips;
  genesis_ = addVEEs(Dag::GENESIS, pivot, tips);
}
Dag::~Dag() {}
void Dag::setVerbose(bool verbose) { verbose_ = verbose; }
void Dag::setDebug(bool debug) { debug_ = debug; }

uint64_t Dag::getNumVertices() const {
  ulock lock(mutex_);
  return boost::num_vertices(graph_);
}
uint64_t Dag::getNumEdges() const {
  ulock lock(mutex_);
  return boost::num_edges(graph_);
}

bool Dag::hasVertex(vertex_hash const &v) const {
  ulock lock(mutex_);
  return graph_.vertex(v) != graph_.null_vertex();
}

void Dag::collectTotalLeaves(std::vector<vertex_hash> &tips) const {
  ulock lock(mutex_);
  vertex_name_map_const_t name_map = boost::get(boost::vertex_name, graph_);
  std::vector<vertex_t> leaves;
  collectLeafVertices(leaves);
  for (auto const &leaf : leaves) {
    tips.push_back(name_map[leaf]);
  }
}

bool Dag::addVEEs(vertex_hash const &new_vertex, vertex_hash const &pivot,
                  std::vector<vertex_hash> const &tips) {
  ulock lock(mutex_);
  assert(!new_vertex.empty());

  // add vertex
  auto now(std::chrono::system_clock::now());
  vertex_t ret = add_vertex(new_vertex, graph_);
  vertex_name_map_t name_map = boost::get(boost::vertex_name, graph_);
  name_map[ret] = new_vertex;
  vertex_time_stamp_map_t stamp_map = boost::get(boost::vertex_index1, graph_);
  stamp_map[ret] = getTimePoint2Long(now);
  // std::cout<<"Created vertex "<< new_vertex<< " at "<< getTimePoint2Long(now)
  // <<std::endl;
  edge_t edge;
  bool res;

  // Note: add edges,
  // *** important
  // Add a new block, edges are pointing from pivot to new_veretx
  if (!pivot.empty()) {
    std::tie(edge, res) = add_edge_by_label(pivot, new_vertex, graph_);
    if (!res) {
      std::cout << "Warning! creating pivot edge \n"
                << pivot << "\n-->\n"
                << new_vertex << " \nunsuccessful!" << std::endl;
    }
  }
  bool res2;
  for (auto const &e : tips) {
    std::tie(edge, res2) = add_edge_by_label(e, new_vertex, graph_);
    if (!res2) {
      std::cout << "Warning! creating tip edge \n"
                << e << "\n-->\n"
                << new_vertex << " \nunsuccessful!" << std::endl;
    }
  }
  return res & res2;
}

void Dag::drawGraph(std::string filename) const {
  ulock lock(mutex_);
  std::ofstream outfile(filename.c_str());
  auto name_map = boost::get(boost::vertex_name, graph_);
  boost::write_graphviz(outfile, graph_, make_label_writer(name_map));
  std::cout << "Dot file " << filename << " generated!" << std::endl;
  std::cout << "Use \"dot -Tpdf <dot file> -o <pdf file>\" to generate pdf file"
            << std::endl;
}

Dag::vertex_t Dag::addVertex(vertex_hash const &hash) {
  vertex_t ret = add_vertex(hash, graph_);
  vertex_name_map_t name_map = boost::get(boost::vertex_name, graph_);
  name_map[ret] = hash;
  return ret;
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
      leaves.push_back(*s);
    }
  }
  assert(leaves.size());
}

void Dag::getChildrenBeforeTimeStamp(vertex_hash const &vertex,
                                     time_stamp_t stamp,
                                     std::vector<vertex_hash> &children) const {
  ulock lock(mutex_);
  vertex_t current = graph_.vertex(vertex);
  if (current == graph_.null_vertex()) {
    std::cout << "Warning! cannot find vertex " << vertex << "\n";
    return;
  }
  children.clear();

  vertex_time_stamp_map_const_t time_map =
      boost::get(boost::vertex_index1, graph_);
  vertex_name_map_const_t name_map = boost::get(boost::vertex_name, graph_);
  vertex_adj_iter_t s, e;
  for (std::tie(s, e) = adjacenct_vertices(current, graph_); s != e; s++) {
    if (time_map[*s] < stamp) {
      children.push_back(name_map[*s]);
    }
  }
}

void Dag::getSubtreeBeforeTimeStamp(vertex_hash const &vertex,
                                    time_stamp_t stamp,
                                    std::vector<vertex_hash> &subtree) const {
  ulock lock(mutex_);
  vertex_t current = graph_.vertex(vertex);
  if (current == graph_.null_vertex()) {
    std::cout << "Warning! cannot find vertex " << vertex << "\n";
    return;
  }
  subtree.clear();
  std::deque<vertex_t> q;
  std::set<vertex_t> visited;
  visited.insert(current);
  q.push_back(current);
  vertex_time_stamp_map_const_t time_map =
      boost::get(boost::vertex_index1, graph_);
  vertex_name_map_const_t name_map = boost::get(boost::vertex_name, graph_);
  vertex_adj_iter_t s, e;

  while (!q.empty()) {
    current = q.front();
    q.pop_front();
    for (std::tie(s, e) = adjacenct_vertices(current, graph_); s != e; s++) {
      if (visited.count(*s)) {
        continue;
      }
      if (time_map[*s] >= stamp) {
        continue;
      }
      visited.insert(*s);
      q.push_back(*s);
      subtree.push_back(name_map[*s]);
    }
  }
}

void Dag::getLeavesBeforeTimeStamp(vertex_hash const &vertex,
                                   time_stamp_t stamp,
                                   std::vector<vertex_hash> &tips) const {
  ulock lock(mutex_);
  vertex_t current = graph_.vertex(vertex);
  if (current == graph_.null_vertex()) {
    std::cout << "Warning! cannot find vertex " << vertex << "\n";
    return;
  }
  tips.clear();
  vertex_time_stamp_map_const_t time_map =
      boost::get(boost::vertex_index1, graph_);
  vertex_name_map_const_t name_map = boost::get(boost::vertex_name, graph_);
  vertex_adj_iter_t s, e;
  std::unordered_set<vertex_t> visited;
  std::queue<vertex_t> qu;
  visited.insert(current);
  qu.emplace(current);
  while (!qu.empty()) {
    vertex_t c = qu.front();
    qu.pop();
    size_t valid_children = 0;
    for (std::tie(s, e) = adjacenct_vertices(c, graph_); s != e; s++) {
      if (time_map[*s] < stamp) {
        valid_children++;
      }

      if (visited.count(*s)) {
        continue;
      }

      visited.insert(*s);

      // old children, still need to explore children
      if (time_map[*s] < stamp) {
        qu.push(*s);
      }
    }
    // time sense leaf
    if (valid_children == 0 && time_map[c] < stamp) {
      tips.push_back(name_map[c]);
    }
  }
}

void Dag::getEpochVertices(vertex_hash const &from, vertex_hash const &to,
                           std::vector<vertex_hash> &epochs) const {
  ulock lock(mutex_);

  vertex_t source = graph_.vertex(from);
  vertex_t target = graph_.vertex(to);

  if (source == graph_.null_vertex()) {
    std::cout << "Warning! cannot find vertex (from)" << from << "\n";
    return;
  }
  if (target == graph_.null_vertex()) {
    std::cout << "Warning! cannot find vertex (to) " << to << "\n";
    return;
  }
  epochs.clear();

  vertex_iter_t s, e;
  vertex_name_map_const_t name_map = boost::get(boost::vertex_name, graph_);

  // iterator all vertex
  for (std::tie(s, e) = boost::vertices(graph_); s != e; ++s) {
    if (*s == source || *s == target) continue;
    if (!reachable(*s, source) && reachable(*s, target)) {
      epochs.push_back(name_map[*s]);
    }
  }
}

time_stamp_t Dag::getVertexTimeStamp(vertex_hash const &vertex) const {
  ulock lock(mutex_);
  vertex_t current = graph_.vertex(vertex);
  if (current == graph_.null_vertex()) {
    std::cout << "Warning! cannot find vertex " << vertex << "\n";
    return 0;
  }
  vertex_time_stamp_map_const_t time_map =
      boost::get(boost::vertex_index1, graph_);
  return time_map[current];
}

void Dag::setVertexTimeStamp(vertex_hash const &vertex, time_stamp_t stamp) {
  ulock lock(mutex_);
  vertex_t current = graph_.vertex(vertex);
  if (current == graph_.null_vertex()) {
    std::cout << "Warning! cannot find vertex " << vertex << "\n";
    return;
  }
  vertex_time_stamp_map_t time_map = boost::get(boost::vertex_index1, graph_);
  assert(stamp >= 0);
  time_map[current] = stamp;
}

size_t Dag::outDegreeBeforeTimeStamp(vertex_t vertex,
                                     time_stamp_t stamp) const {
  size_t deg = 0;
  vertex_time_stamp_map_const_t time_map =
      boost::get(boost::vertex_index1, graph_);
  vertex_adj_iter_t s, e;
  for (std::tie(s, e) = adjacenct_vertices(vertex, graph_); s != e; s++) {
    if (time_map[*s] < stamp) {
      deg++;
    }
  }
  return deg;
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

void PivotTree::getHeavySubtreePath(
    vertex_hash const &vertex, std::vector<vertex_hash> &pivot_chain) const {
  return getHeavySubtreePathBeforeTimeStamp(
      vertex, std::numeric_limits<uint64_t>::max(), pivot_chain);
}

/**
 * Iterative version
 * Steps rounds
 * 1. post order traversal
 * 2. from leave, count weight and propagate up
 * 3. collect path
 */

void PivotTree::getHeavySubtreePathBeforeTimeStamp(
    vertex_hash const &vertex, time_stamp_t stamp,
    std::vector<vertex_hash> &pivot_chain) const {
  ulock lock(mutex_);
  std::vector<vertex_t> post_order;
  vertex_t root = graph_.vertex(vertex);

  if (root == graph_.null_vertex()) {
    std::cout << "Warning! cannot find vertex " << vertex << "\n";
    return;
  }
  pivot_chain.clear();
  if (outDegreeBeforeTimeStamp(root, stamp) == 0) {
    return;
  }
  vertex_time_stamp_map_const_t time_map =
      boost::get(boost::vertex_index1, graph_);

  // post order traversal
  std::stack<vertex_t> st;
  st.emplace(root);
  vertex_t cur;
  vertex_adj_iter_t s, e;
  while (!st.empty()) {
    cur = st.top();
    st.pop();
    post_order.emplace_back(cur);
    // visit only smaller time stamp
    for (std::tie(s, e) = adjacenct_vertices(cur, graph_); s != e; s++) {
      if (time_map[*s] < stamp) {
        st.emplace(*s);
      }
    }
  }
  std::reverse(post_order.begin(), post_order.end());

  // compute weight
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

  vertex_name_map_const_t name_map = boost::get(boost::vertex_name, graph_);
  // collect path
  while (1) {
    pivot_chain.emplace_back(name_map[root]);
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
        if (name_map[*s] < name_map[next]) {
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

DagManager::DagManager(unsigned num_threads) try
    : debug_(false),
      verbose_(false),
      dag_updated_(false),
      num_threads_(num_threads),
      inserting_index_counter_(0),
      total_dag_(std::make_shared<Dag>()),
      pivot_tree_(std::make_shared<PivotTree>()),
      sb_buffer_array_(std::make_shared<std::vector<DagBuffer>>(num_threads)) {
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

void DagManager::setDebug(bool debug) { debug_ = debug; }
void DagManager::setVerbose(bool verbose) { verbose_ = verbose; }

std::pair<uint64_t, uint64_t> DagManager::getNumVerticesInDag() const {
  ulock lock(mutex_);
  return {total_dag_->getNumVertices(), pivot_tree_->getNumVertices()};
}

std::pair<uint64_t, uint64_t> DagManager::getNumEdgesInDag() const {
  ulock lock(mutex_);
  return {total_dag_->getNumEdges(), pivot_tree_->getNumEdges()};
}
size_t DagManager::getBufferSize() const {
  ulock lock(mutex_);
  auto sz = 0;
  for (auto const &arr : (*sb_buffer_array_)) {
    sz += arr.size();
  }
  return sz;
}

unsigned DagManager::getBlockInsertingIndex() {
  auto which_buffer = inserting_index_counter_.fetch_add(1);
  assert(which_buffer < num_threads_ && which_buffer >= 0);

  if (inserting_index_counter_ >= num_threads_) {
    inserting_index_counter_.store(0);
  }
  if (verbose_) {
    std::cout << "inserting row = " << which_buffer << "\n";
  }
  return which_buffer;
}

void DagManager::drawTotalGraph(std::string const &str) const {
  ulock lock(mutex_);
  total_dag_->drawGraph(str);
}

void DagManager::drawPivotGraph(std::string const &str) const {
  ulock lock(mutex_);
  pivot_tree_->drawGraph(str);
}

void DagManager::start() {
  if (!stopped_) return;
  stopped_ = false;
  for (auto i = 0; i < num_threads_; ++i) {
    sb_buffer_processing_threads_.push_back(
        boost::thread([this, i]() { consume(i); }));
  }
}
void DagManager::stop() {
  if (stopped_) return;
  stopped_ = true;
  for (auto &arr : (*sb_buffer_array_)) {
    arr.stop();
  }

  for (auto &t : sb_buffer_processing_threads_) {
    t.join();
  }
}
bool DagManager::addDagBlock(DagBlock const &blk, bool insert) {
  ulock lock(mutex_);
  std::string hash = blk.getHash().toString();
  if (total_dag_->hasVertex(hash)) {
    if (verbose_) {
      std::cout << "Block is in DAG already! " << hash << std::endl;
    }
    return false;
  }

  std::string pivot = blk.getPivot().toString();
  if (!total_dag_->hasVertex(pivot)) {
    if (verbose_) {
      std::cout << "Block " << hash << " pivot " << pivot
                << " unavailable, insert = " << insert << std::endl;
    }
    if (insert) {
      addToDagBuffer(blk);
    }
    return false;
  }

  std::vector<std::string> tips;
  for (auto const &t : blk.getTips()) {
    std::string tip = t.toString();
    if (!total_dag_->hasVertex(tip)) {
      if (verbose_) {
        std::cout << "Block " << hash << " tip " << tip
                  << " unavailable, insert = " << insert << std::endl;
      }
      if (insert) {
        addToDagBuffer(blk);
      }
      return false;
    }
    tips.push_back(tip);
  }

  addToDag(hash, pivot, tips);
  return true;
}

void DagManager::addToDagBuffer(DagBlock const &blk) {
  unsigned which_buffer = getBlockInsertingIndex();
  (*sb_buffer_array_)[which_buffer].insert(blk);
}

void DagManager::addToDag(std::string const &hash, std::string const &pivot,
                          std::vector<std::string> const &tips) {
  total_dag_->addVEEs(hash, pivot, tips);
  pivot_tree_->addVEEs(hash, pivot, {});
  // sync pivot tree's time stamp with total_dag_ timestamp
  auto stamp = total_dag_->getVertexTimeStamp(hash);
  pivot_tree_->setVertexTimeStamp(hash, stamp);
}

/**
 * TODO: current will do spining access, change it
 */

void DagManager::consume(unsigned idx) {
  while (!stopped_) {
    auto iter = (*sb_buffer_array_)[idx].getBuffer();
    auto &blk = iter->first;
    if (stopped_) break;
    if (addDagBlock(blk, false)) {
      // std::cout << "consume success ..." << blk.getHash().toString()
      //           << std::endl;
      (*sb_buffer_array_)[idx].delBuffer(iter);
    }
  }
}
bool DagManager::getLatestPivotAndTips(std::string &pivot,
                                       std::vector<std::string> &tips) const {
  bool ret = false;
  // make sure the state of dag is the same when collection pivot and tips
  ulock lock(mutex_);
  std::vector<std::string> pivot_chain;
  pivot.clear();
  tips.clear();
  pivot_tree_->getHeavySubtreePathBeforeTimeStamp(
      Dag::GENESIS, std::numeric_limits<uint64_t>::max(), pivot_chain);
  if (!pivot_chain.empty()) {
    pivot = pivot_chain.back();
    total_dag_->collectTotalLeaves(tips);
    ret = true;
  }
  return ret;
}

void DagManager::collectTotalLeaves(std::vector<std::string> &leaves) const {
  total_dag_->collectTotalLeaves(leaves);
}

std::vector<std::string> DagManager::getPivotChildrenBeforeTimeStamp(
    std::string const &vertex, time_stamp_t stamp) const {
  std::vector<std::string> ret;
  pivot_tree_->getChildrenBeforeTimeStamp(vertex, stamp, ret);
  return ret;
}

std::vector<std::string> DagManager::getTotalChildrenBeforeTimeStamp(
    std::string const &vertex, time_stamp_t stamp) const {
  std::vector<std::string> ret;
  total_dag_->getChildrenBeforeTimeStamp(vertex, stamp, ret);
  return ret;
}

std::vector<std::string> DagManager::getPivotSubtreeBeforeTimeStamp(
    std::string const &vertex, time_stamp_t stamp) const {
  std::vector<std::string> ret;
  pivot_tree_->getSubtreeBeforeTimeStamp(vertex, stamp, ret);
  return ret;
}

std::vector<std::string> DagManager::getTotalLeavesBeforeTimeStamp(
    std::string const &vertex, time_stamp_t stamp) const {
  std::vector<std::string> ret;
  total_dag_->getLeavesBeforeTimeStamp(vertex, stamp, ret);
  return ret;
}

std::vector<std::string> DagManager::getTotalEpochsBetweenBlocks(
    std::string const &from, std::string const &to) const {
  std::vector<std::string> ret;
  total_dag_->getEpochVertices(from, to, ret);
  return ret;
}

std::vector<std::string> DagManager::getPivotChainBeforeTimeStamp(
    std::string const &vertex, time_stamp_t stamp) const {
  std::vector<std::string> ret;
  pivot_tree_->getHeavySubtreePathBeforeTimeStamp(vertex, stamp, ret);
  return ret;
}

time_stamp_t DagManager::getDagBlockTimeStamp(std::string const &vertex) {
  return total_dag_->getVertexTimeStamp(vertex);
}

void DagManager::setDagBlockTimeStamp(std::string const &vertex,
                                      time_stamp_t stamp) {
  total_dag_->setVertexTimeStamp(vertex, stamp);
  pivot_tree_->setVertexTimeStamp(vertex, stamp);
}

DagBuffer::DagBuffer() : stopped_(true), updated_(false), iter_(blocks_.end()) {
  if (stopped_) {
    start();
  }
}

void DagBuffer::start() { stopped_ = false; }
void DagBuffer::stop() {
  if (stopped_) {
    return;
  }
  stopped_ = true;
  condition_.notify_all();
}

bool DagBuffer::isStopped() const { return stopped_; }

void DagBuffer::insert(DagBlock const &blk) {
  ulock lock(mutex_);
  time_point_t t = std::chrono::system_clock::now();
  blocks_.emplace_front(std::make_pair(blk, t));
  updated_ = true;
  condition_.notify_one();
}

DagBuffer::buffIter DagBuffer::getBuffer() {
  ulock lock(mutex_);
  while (!stopped_ &&
         (blocks_.empty() || (iter_ == blocks_.end() && !updated_))) {
    condition_.wait(lock);
  }
  if (iter_ == blocks_.end()) {
    iter_ = blocks_.begin();
  }
  auto cur = iter_;
  iter_++;
  return cur;
}
void DagBuffer::delBuffer(DagBuffer::buffIter iter) {
  ulock lock(mutex_);
  blocks_.erase(iter);
}

size_t DagBuffer::size() const { return blocks_.size(); }

TipBlockExplorer::TipBlockExplorer(unsigned rate)
    : rate_limit_(rate), counter_(0) {
  start();
}

void TipBlockExplorer::start() { stopped_ = false; }
// only one DagManager will call the function
void TipBlockExplorer::blockAdded() {
  ulock lock(mutex_);
  counter_++;
  if (counter_ >= rate_limit_) {
    ready_ = true;
    counter_ = 0;
    condition_.notify_one();
  } else {
    ready_ = false;
  }
}

TipBlockExplorer::~TipBlockExplorer() {
  if (!stopped_) {
    stop();
  }
}
void TipBlockExplorer::stop() {
  if (stopped_) return;
  stopped_ = true;
  condition_.notify_all();
}
bool TipBlockExplorer::waitForReady() {
  ulock lock(mutex_);
  while (!stopped_ && !ready_) {
    condition_.wait(lock);
  }
  bool ret = true;
  if (stopped_) {
    ret = false;
  }
  ready_ = false;
  return ret;
}

}  // namespace taraxa