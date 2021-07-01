#pragma once

#include <atomic>
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
#include <chrono>
#include <condition_variable>
#include <iostream>
#include <iterator>
#include <list>
#include <mutex>
#include <queue>
#include <string>

#include "common/types.hpp"
#include "consensus/pbft_chain.hpp"
#include "dag_block.hpp"
#include "storage/db_storage.hpp"
#include "transaction_manager/transaction_manager.hpp"
#include "util/util.hpp"
namespace taraxa {

/**
 * Thread safe. Labelled graph.
 */
class DagManager;
class Dag {
 public:
  // properties
  using vertex_hash = std::string;
  using vertex_property_t =
      boost::property<boost::vertex_index_t, std::string, boost::property<boost::vertex_index1_t, uint64_t>>;
  using edge_property_t = boost::property<boost::edge_index_t, uint64_t>;

  // graph def
  using adj_list_t =
      boost::adjacency_list<boost::setS, boost::hash_setS, boost::directedS, vertex_property_t, edge_property_t>;
  using graph_t = boost::labeled_graph<adj_list_t, std::string, boost::hash_mapS>;
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
  explicit Dag(std::string const &genesis, addr_t node_addr);
  virtual ~Dag() = default;
  uint64_t getNumVertices() const;
  uint64_t getNumEdges() const;
  bool hasVertex(vertex_hash const &v) const;
  bool addVEEs(vertex_hash const &new_vertex, vertex_hash const &pivot, std::vector<vertex_hash> const &tips);

  void getLeaves(std::vector<vertex_hash> &tips) const;
  void drawGraph(vertex_hash filename) const;

  bool computeOrder(vertex_hash const &anchor, std::vector<vertex_hash> &ordered_period_vertices,
                    std::map<uint64_t, std::vector<std::string>> const &non_finalized_blks);

  void clear();

 protected:
  // Note: private functions does not lock

  // edge API
  edge_t addEdge(std::string const &v1, std::string const &v2);
  edge_t addEdge(vertex_t v1, vertex_t v2);

  // traverser API
  bool reachable(vertex_t const &from, vertex_t const &to) const;

  void collectLeafVertices(std::vector<vertex_t> &leaves) const;

  graph_t graph_;
  vertex_t genesis_;  // root node

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
  explicit PivotTree(std::string const &genesis, addr_t node_addr) : Dag(genesis, node_addr){};
  virtual ~PivotTree() = default;
  using vertex_t = Dag::vertex_t;
  using vertex_adj_iter_t = Dag::vertex_adj_iter_t;
  using vertex_index_map_const_t = Dag::vertex_index_map_const_t;

  void getGhostPath(vertex_hash const &vertex, std::vector<vertex_hash> &pivot_chain) const;
};
class DagBuffer;
class FullNode;

class DagManager : public std::enable_shared_from_this<DagManager> {
 public:
  using uLock = boost::unique_lock<boost::shared_mutex>;
  using sharedLock = boost::shared_lock<boost::shared_mutex>;

  explicit DagManager(std::string const &genesis, addr_t node_addr, std::shared_ptr<TransactionManager> trx_mgr,
                      std::shared_ptr<PbftChain> pbft_chain, std::shared_ptr<DbStorage> db);
  virtual ~DagManager() = default;
  std::shared_ptr<DagManager> getShared();
  void stop();

  std::string const &get_genesis() { return genesis_; }

  bool pivotAndTipsAvailable(DagBlock const &blk);
  void addDagBlock(DagBlock const &blk, bool finalized = false,
                   bool save = true);  // insert to buffer if fail

  // return {period, block order}, for pbft-pivot-blk proposing (does not
  // finalize)
  std::pair<uint64_t, std::shared_ptr<vec_blk_t>> getDagBlockOrder(blk_hash_t const &anchor);
  // receive pbft-povit-blk, update periods and finalized, return size of
  // ordered blocks
  uint setDagBlockOrder(blk_hash_t const &anchor, uint64_t period, vec_blk_t const &dag_order,
                        DbStorage::Batch &write_batch);

  bool getLatestPivotAndTips(std::string &pivot, std::vector<std::string> &tips) const;
  void collectTotalLeaves(std::vector<std::string> &leaves) const;

  void getGhostPath(std::string const &source, std::vector<std::string> &ghost) const;
  void getGhostPath(std::vector<std::string> &ghost) const;  // get ghost path from last anchor
  // ----- Total graph
  void drawTotalGraph(std::string const &str) const;

  // ----- Pivot graph
  // can return self as pivot chain
  void drawPivotGraph(std::string const &str) const;
  void drawGraph(std::string const &dotfile) const;

  std::pair<uint64_t, uint64_t> getNumVerticesInDag() const;
  std::pair<uint64_t, uint64_t> getNumEdgesInDag() const;
  level_t getMaxLevel() const { return max_level_; }

  // DAG anchors
  uint64_t getLatestPeriod() const {
    sharedLock lock(mutex_);
    return period_;
  }
  std::pair<std::string, std::string> getAnchors() const {
    sharedLock lock(mutex_);
    return std::make_pair(old_anchor_, anchor_);
  }

  const std::map<uint64_t, std::vector<std::string>> &getNonFinalizedBlocks() const;

  DagFrontier getDagFrontier();

  /**
   * @return std::pair<size_t, size_t> -> first = number of levels, second = number of blocks
   */
  std::pair<size_t, size_t> getNonFinalizedBlocksSize() const;

  /**
   * @return std::pair<size_t, size_t> -> first = number of levels, second = number of blocks
   */
  std::pair<size_t, size_t> getFinalizedBlocksSize() const;

 private:
  void recoverDag();
  void addToDag(std::string const &hash, std::string const &pivot, std::vector<std::string> const &tips, uint64_t level,
                DbStorage::Batch &write_batch, bool finalized = false);
  std::pair<std::string, std::vector<std::string>> getFrontier() const;  // return pivot and tips
  std::atomic<level_t> max_level_ = 0;
  mutable boost::shared_mutex mutex_;
  std::shared_ptr<PivotTree> pivot_tree_;  // only contains pivot edges
  std::shared_ptr<Dag> total_dag_;         // contains both pivot and tips
  std::shared_ptr<TransactionManager> trx_mgr_;
  std::shared_ptr<PbftChain> pbft_chain_;
  std::shared_ptr<DbStorage> db_;
  std::string anchor_;      // anchor of the last period
  std::string old_anchor_;  // anchor of the second to last period
  uint64_t period_;         // last period
  std::string genesis_;
  std::map<uint64_t, std::vector<std::string>> non_finalized_blks_;
  std::map<uint64_t, std::vector<std::string>> finalized_blks_;
  DagFrontier frontier_;
  LOG_OBJECTS_DEFINE
};

// for graphviz
template <class Property1>
class vertex_label_writer {
 public:
  vertex_label_writer(Property1 name) : name(name) {}
  template <class Vertex>
  void operator()(std::ostream &out, const Vertex &v) const {
    out << "[label=\"" << name[v].substr(0, 8) << " "
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

}  // namespace taraxa
