#ifndef TARAXA_NODE_DAG_HPP
#define TARAXA_NODE_DAG_HPP

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
#include <string>
#include "dag_block.hpp"
#include "libdevcore/Log.h"
#include "types.hpp"
#include "util.hpp"

namespace taraxa {

/**
 * Thread safe. Labelled graph.
 */

class Dag {
 public:
  // properties
  using vertex_hash = std::string;
  using vertex_property_t =
      boost::property<boost::vertex_index_t, std::string,
                      boost::property<boost::vertex_index1_t, uint64_t>>;
  using edge_property_t = boost::property<boost::edge_index_t, uint64_t>;

  // graph def
  using adj_list_t =
      boost::adjacency_list<boost::setS, boost::hash_setS, boost::directedS,
                            vertex_property_t, edge_property_t>;
  using graph_t =
      boost::labeled_graph<adj_list_t, std::string, boost::hash_mapS>;
  using vertex_t = boost::graph_traits<graph_t>::vertex_descriptor;
  using edge_t = boost::graph_traits<graph_t>::edge_descriptor;
  using vertex_iter_t = boost::graph_traits<graph_t>::vertex_iterator;
  using edge_iter_t = boost::graph_traits<graph_t>::edge_iterator;
  using vertex_adj_iter_t = boost::graph_traits<graph_t>::adjacency_iterator;

  // property_map
  using vertex_index_map_const_t =
      boost::property_map<graph_t, boost::vertex_index_t>::const_type;
  using vertex_index_map_t =
      boost::property_map<graph_t, boost::vertex_index_t>::type;

  using vertex_period_map_const_t =
      boost::property_map<graph_t, boost::vertex_index1_t>::const_type;
  using vertex_period_map_t =
      boost::property_map<graph_t, boost::vertex_index1_t>::type;

  using edge_index_map_const_t =
      boost::property_map<graph_t, boost::edge_index_t>::const_type;
  using edge_index_map_t =
      boost::property_map<graph_t, boost::edge_index_t>::type;

  using uLock = boost::unique_lock<boost::shared_mutex>;
  using sharedLock = boost::shared_lock<boost::shared_mutex>;
  using upgradableLock = boost::upgrade_lock<boost::shared_mutex>;
  using upgradeLock = boost::upgrade_to_unique_lock<boost::shared_mutex>;

  Dag(std::string const &genesis);
  ~Dag();
  static vertex_hash getGenesis();
  uint64_t getNumVertices() const;
  uint64_t getNumEdges() const;
  bool hasVertex(vertex_hash const &v) const;
  // VEE: new vertex, pivot, tips
  // Note: *** The function does not check vertex existency
  bool addVEEs(vertex_hash const &new_vertex, vertex_hash const &pivot,
               std::vector<vertex_hash> const &tips);

  void getLeaves(std::vector<vertex_hash> &tips) const;
  void drawGraph(vertex_hash filename) const;

  // Note, the function will not delete recent_added_blks when marking
  // ith_number
  bool computeOrder(bool finialized, vertex_hash const &anchor,
                    uint64_t ith_peroid,
                    std::unordered_set<vertex_hash>
                        &recent_added_blks,  // iterater only from new blocks
                    std::vector<vertex_hash> &ordered_period_vertices);
  // warning! slow, iterate through all vertices ...
  void getEpFriendVertices(vertex_hash const &from, vertex_hash const &to,
                           std::vector<vertex_hash> &epfriend) const;

  // deleter
  std::vector<Dag::vertex_hash> deletePeriod(uint64_t period);
  void delVertex(std::string const &v);

  // properties
  uint64_t getVertexPeriod(vertex_hash const &vertex) const;
  void setVertexPeriod(vertex_hash const &vertex, uint64_t period);

 protected:
  // Note: private functions does not lock

  // vertex API
  vertex_t addVertex(std::string const &v);
  // will delete all edges associate with the vertex

  // edge API
  edge_t addEdge(std::string const &v1, std::string const &v2);
  edge_t addEdge(vertex_t v1, vertex_t v2);

  // traverser API
  bool reachable(vertex_t const &from, vertex_t const &to) const;

  void collectLeafVertices(std::vector<vertex_t> &leaves) const;

  graph_t graph_;
  std::unordered_map<uint64_t, std::unordered_set<vertex_hash>> periods_;
  vertex_t genesis_;  // root node
  mutable boost::shared_mutex mutex_;

 private:
  mutable dev::Logger log_er_{
      dev::createLogger(dev::Verbosity::VerbosityError, "DAGMGR")};
  mutable dev::Logger log_wr_{
      dev::createLogger(dev::Verbosity::VerbosityWarning, "DAGMGR")};
  mutable dev::Logger log_nf_{
      dev::createLogger(dev::Verbosity::VerbosityInfo, "DAGMGR")};
  mutable dev::Logger log_tr_{
      dev::createLogger(dev::Verbosity::VerbosityTrace, "DAGMGR")};
};

/**
 * PivotTree is a special DAG, every vertex only has one out-edge,
 * therefore, there is no convergent tree
 */

class PivotTree : public Dag {
 public:
  PivotTree(std::string const &genesis) : Dag(genesis){};
  using vertex_t = Dag::vertex_t;
  using vertex_adj_iter_t = Dag::vertex_adj_iter_t;
  using vertex_index_map_const_t = Dag::vertex_index_map_const_t;

  void getGhostPath(vertex_hash const &vertex,
                    std::vector<vertex_hash> &pivot_chain) const;

 private:
  mutable dev::Logger log_er_{
      dev::createLogger(dev::Verbosity::VerbosityError, "DAGMGR")};
  mutable dev::Logger log_wr_{
      dev::createLogger(dev::Verbosity::VerbosityWarning, "DAGMGR")};
  mutable dev::Logger log_nf_{
      dev::createLogger(dev::Verbosity::VerbosityInfo, "DAGMGR")};
  mutable dev::Logger log_tr_{
      dev::createLogger(dev::Verbosity::VerbosityTrace, "DAGMGR")};
};
class DagBuffer;
/**
 * Important : The input DagBlocks to DagManger should be de-duplicated!
 * i.e., no same DagBlocks are put to the Dag.
 */

// TODO: probably share property map for total_dag_ and pivot_tree_

class FullNode;

class DagManager : public std::enable_shared_from_this<DagManager> {
 public:
  using stdLock = std::unique_lock<std::mutex>;
  using uLock = boost::unique_lock<boost::shared_mutex>;
  using sharedLock = boost::shared_lock<boost::shared_mutex>;
  using upgradableLock = boost::upgrade_lock<boost::shared_mutex>;
  using upgradeLock = boost::upgrade_to_unique_lock<boost::shared_mutex>;

  DagManager(std::string const &genesis);
  virtual ~DagManager();
  void start();
  void stop();
  std::shared_ptr<DagManager> getShared();
  void setFullNode(std::shared_ptr<FullNode> full_node) {
    full_node_ = full_node;
  }
  bool addDagBlock(DagBlock const &blk);  // insert to buffer if fail
  
  // use a anchor to create period, return current_period, does not finalize
  uint64_t getDagBlockOrder(blk_hash_t const &anchor, vec_blk_t &orders);
  // assuming a period is confirmed, will finialize, return size of blocks in
  // the period
  uint setDagBlockPeriod(blk_hash_t const &anchor, uint64_t period);

  bool getLatestPivotAndTips(std::string &pivot,
                             std::vector<std::string> &tips) const;
  void collectTotalLeaves(std::vector<std::string> &leaves) const;

  void getGhostPath(std::string const &source,
                    std::vector<std::string> &ghost) const;
  void deletePeriod(uint64_t period);
  // ----- Total graph
  std::vector<std::string> getEpFriendBetweenPivots(std::string const &from,
                                                    std::string const &to);
  void drawTotalGraph(std::string const &str) const;

  // ----- Pivot graph
  // can return self as pivot chain
  std::vector<std::string> getPivotChain(std::string const &vertex) const;
  void drawPivotGraph(std::string const &str) const;
  std::pair<uint64_t, uint64_t> getNumVerticesInDag() const;
  std::pair<uint64_t, uint64_t> getNumEdgesInDag() const;
  level_t getMaxLevel() const { return max_level_; }
  uint64_t getLatestPeriod() const { return anchors_.size() - 1; }
  std::string getLatestAnchor() const { return anchors_.back(); }

 private:
  size_t num_cached_period_in_dag = 1000;
  void addToDag(std::string const &hash, std::string const &pivot,
                std::vector<std::string> const &tips);
  unsigned getBlockInsertingIndex();  // add to block to different array
  addr_t getFullNodeAddress() const;
  bool stopped_ = true;
  std::weak_ptr<FullNode> full_node_;
  level_t max_level_ = 0;
  mutable boost::shared_mutex mutex_;
  std::atomic<unsigned> inserting_index_counter_;
  std::shared_ptr<PivotTree> pivot_tree_;  // only contains pivot edges
  std::shared_ptr<Dag> total_dag_;         // contains both pivot and tips
  std::unordered_set<std::string> recent_added_blks_;
  std::vector<std::string> anchors_;  // pivots that define periods
  std::string genesis_;
  dev::Logger log_er_{
      dev::createLogger(dev::Verbosity::VerbosityError, "DAGMGR")};
  dev::Logger log_wr_{
      dev::createLogger(dev::Verbosity::VerbosityWarning, "DAGMGR")};
  dev::Logger log_nf_{
      dev::createLogger(dev::Verbosity::VerbosityInfo, "DAGMGR")};
  dev::Logger log_dg_{
      dev::createLogger(dev::Verbosity::VerbosityDebug, "DAGMGR")};
  dev::Logger log_tr_{
      dev::createLogger(dev::Verbosity::VerbosityTrace, "DAGMGR")};
};

// for graphviz
template <class Property1, class Property2>
class vertex_label_writer {
 public:
  vertex_label_writer(Property1 name, Property2 period)
      : name(name), period(period) {}
  template <class Vertex>
  void operator()(std::ostream &out, const Vertex &v) const {
    out << "[label=\"" << name[v].substr(0, 6) << "..." << name[v].substr(58)
        << " (" << period[v] << ") "
        << "\"]";
  }

 private:
  Property1 name;
  Property2 period;
};

template <class Property>
class edge_label_writer {
 public:
  edge_label_writer(Property weight) : weight(weight) {}
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

#endif  // TARAXA_NODE_DAG_HPP