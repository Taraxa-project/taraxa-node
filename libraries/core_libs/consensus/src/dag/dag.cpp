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
#include "key_manager/key_manager.hpp"
#include "network/network.hpp"
#include "network/tarcap/packets_handlers/dag_block_packet_handler.hpp"
#include "transaction/transaction_manager.hpp"

#define NULL_BLOCK_HASH blk_hash_t(0)

namespace taraxa {

Dag::Dag(blk_hash_t const &dag_genesis_block_hash, addr_t node_addr) {
  LOG_OBJECTS_CREATE("DAGMGR");
  std::vector<blk_hash_t> tips;
  // add genesis block
  addVEEs(dag_genesis_block_hash, {}, tips);
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
}  // namespace taraxa
