#pragma once

#include <bitset>
#include <boost/function.hpp>
#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/breadth_first_search.hpp>
#include <boost/graph/depth_first_search.hpp>
#include <boost/graph/graph_traits.hpp>
#include <boost/graph/graphviz.hpp>
#include <boost/graph/labeled_graph.hpp>
#include <boost/graph/properties.hpp>
#include <boost/property_map/property_map.hpp>
#include <boost/thread.hpp>
#include <iostream>
#include <iterator>
#include <list>
#include <mutex>
#include <queue>
#include <string>

#include "common/types.hpp"
#include "common/util.hpp"
#include "dag/dag_block.hpp"
namespace taraxa {

/** @addtogroup DAG
 * @{
 */
class DagManager;
class Network;

/**
 * @brief Thread safe. Labelled graph.
 */
class Dag {
 public:
  // properties
  using vertex_property_t =
      boost::property<boost::vertex_index_t, blk_hash_t, boost::property<boost::vertex_index1_t, uint64_t>>;
  using edge_property_t = boost::property<boost::edge_index_t, uint64_t>;

  // graph def
  using adj_list_t =
      boost::adjacency_list<boost::setS, boost::hash_setS, boost::directedS, vertex_property_t, edge_property_t>;
  using graph_t = boost::labeled_graph<adj_list_t, blk_hash_t, boost::hash_mapS>;
  using vertex_t = boost::graph_traits<graph_t>::vertex_descriptor;
  using edge_t = boost::graph_traits<graph_t>::edge_descriptor;
  using vertex_iter_t = boost::graph_traits<graph_t>::vertex_iterator;
  using edge_iter_t = boost::graph_traits<graph_t>::edge_iterator;
  using vertex_adj_iter_t = boost::graph_traits<graph_t>::adjacency_iterator;

  // property_map
  using vertex_index_map_const_t = boost::property_map<graph_t, boost::vertex_index_t>::const_type;
  using vertex_index_map_t = boost::property_map<graph_t, boost::vertex_index_t>::type;

  using vertex_period_map_const_t = boost::property_map<graph_t, boost::vertex_index1_t>::const_type;
  using vertex_period_map_t = boost::property_map<graph_t, boost::vertex_index1_t>::type;

  using edge_index_map_const_t = boost::property_map<graph_t, boost::edge_index_t>::const_type;
  using edge_index_map_t = boost::property_map<graph_t, boost::edge_index_t>::type;

  friend DagManager;

  explicit Dag(blk_hash_t const &dag_genesis_block_hash, addr_t node_addr);
  virtual ~Dag() = default;

  Dag(const Dag &) = default;
  Dag(Dag &&) = default;
  Dag &operator=(const Dag &) = default;
  Dag &operator=(Dag &&) = default;

  uint64_t getNumVertices() const;
  uint64_t getNumEdges() const;
  bool hasVertex(blk_hash_t const &v) const;
  bool addVEEs(blk_hash_t const &new_vertex, blk_hash_t const &pivot, std::vector<blk_hash_t> const &tips);

  void getLeaves(std::vector<blk_hash_t> &tips) const;
  void drawGraph(std::string const &filename) const;

  bool computeOrder(const blk_hash_t &anchor, std::vector<blk_hash_t> &ordered_period_vertices,
                    const std::map<uint64_t, std::unordered_set<blk_hash_t>> &non_finalized_blks);

  void clear();

 protected:
  // Note: private functions does not lock

  // traverser API
  bool reachable(vertex_t const &from, vertex_t const &to) const;

  void collectLeafVertices(std::vector<vertex_t> &leaves) const;

  graph_t graph_;

 protected:
  LOG_OBJECTS_DEFINE
};

/**
 * PivotTree is a special DAG, every vertex only has one out-edge,
 * therefore, there is no convergent tree
 */

class PivotTree : public Dag {
 public:
  friend DagManager;
  explicit PivotTree(blk_hash_t const &dag_genesis_block_hash, addr_t node_addr)
      : Dag(dag_genesis_block_hash, node_addr) {}
  virtual ~PivotTree() = default;

  PivotTree(const PivotTree &) = default;
  PivotTree(PivotTree &&) = default;
  PivotTree &operator=(const PivotTree &) = default;
  PivotTree &operator=(PivotTree &&) = default;

  using Dag::vertex_adj_iter_t;
  using Dag::vertex_index_map_const_t;
  using Dag::vertex_t;

  void getGhostPath(blk_hash_t const &vertex, std::vector<blk_hash_t> &pivot_chain) const;
};
class DagBuffer;
class FullNode;
class KeyManager;

// for graphviz
template <class Property1>
class vertex_label_writer {
 public:
  vertex_label_writer(Property1 name) : name(name) {}
  template <class Vertex>
  void operator()(std::ostream &out, const Vertex &v) const {
    out << "[label=\"" << name[v].toString().substr(0, 8) << " "
        << "\"]";
  }

 private:
  Property1 name;
};

template <class Property>
class edge_label_writer {
 public:
  explicit edge_label_writer(Property weight) : weight(weight) {}
  template <class Edge>
  void operator()(std::ostream &out, const Edge &e) const {
    if (weight[e] == 0) {
      out << "[style=\"dashed\" dir=\"back\"]";
    } else {
      out << "[dir=\"back\"]";
    }
  }

 private:
  Property weight;
};
/** @}*/

}  // namespace taraxa
