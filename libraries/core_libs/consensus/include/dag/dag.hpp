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
#include "common/util.hpp"
#include "dag/dag_block.hpp"
#include "dag/dag_block_manager.hpp"
#include "pbft/pbft_chain.hpp"
#include "storage/storage.hpp"
#include "transaction/transaction_manager.hpp"
namespace taraxa {

/** \addtogroup DAG
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

  explicit Dag(blk_hash_t const &genesis, addr_t node_addr);
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
  explicit PivotTree(blk_hash_t const &genesis, addr_t node_addr) : Dag(genesis, node_addr) {}
  virtual ~PivotTree() = default;

  PivotTree(const PivotTree &) = default;
  PivotTree(PivotTree &&) = default;
  PivotTree &operator=(const PivotTree &) = default;
  PivotTree &operator=(PivotTree &&) = default;

  using vertex_t = Dag::vertex_t;
  using vertex_adj_iter_t = Dag::vertex_adj_iter_t;
  using vertex_index_map_const_t = Dag::vertex_index_map_const_t;

  void getGhostPath(blk_hash_t const &vertex, std::vector<blk_hash_t> &pivot_chain) const;
};
class DagBuffer;
class FullNode;

class DagManager : public std::enable_shared_from_this<DagManager> {
 public:
  using ULock = std::unique_lock<std::shared_mutex>;
  using SharedLock = std::shared_lock<std::shared_mutex>;

  explicit DagManager(blk_hash_t const &genesis, addr_t node_addr, std::shared_ptr<TransactionManager> trx_mgr,
                      std::shared_ptr<PbftChain> pbft_chain, std::shared_ptr<DagBlockManager> dag_blk_mgr,
                      std::shared_ptr<DbStorage> db, logger::Logger log_time, bool is_light_node = false,
                      uint64_t light_node_history = 0, uint32_t max_levels_per_period = kMaxLevelsPerPeriod,
                      uint32_t dag_expiry_limit = kDagExpiryLevelLimit);
  virtual ~DagManager() { stop(); }

  DagManager(const DagManager &) = delete;
  DagManager(DagManager &&) = delete;
  DagManager &operator=(const DagManager &) = delete;
  DagManager &operator=(DagManager &&) = delete;

  std::shared_ptr<DagManager> getShared();
  void start();
  void stop();
  void setNetwork(std::weak_ptr<Network> network) { network_ = std::move(network); }

  blk_hash_t const &get_genesis() { return genesis_; }

  bool pivotAndTipsAvailable(DagBlock const &blk);
  bool addDagBlock(DagBlock &&blk, SharedTransactions &&trxs = {}, bool proposed = false,
                   bool save = true);  // insert to buffer if fail

  // return block order
  vec_blk_t getDagBlockOrder(blk_hash_t const &anchor, uint64_t period);

  /**
   * @brief Sets the dag block order on finalizing PBFT block
   * IMPORTANT: This method is invoked on finalizing a pbft block, it needs to be protected with mutex_ but the mutex is
   * locked from pbft manager for the entire pbft finalization to make the finalization atomic
   *
   * @param anchor Anchor of the finalized pbft block
   * @param period Period finalized
   * @param dag_order Dag order of the finalized pbft block
   *
   * @return number of dag blocks finalized
   */
  uint setDagBlockOrder(blk_hash_t const &anchor, uint64_t period, vec_blk_t const &dag_order);

  uint64_t getLightNodeHistory() const { return light_node_history_; }
  bool isLightNode() const { return is_light_node_; }

  std::optional<std::pair<blk_hash_t, std::vector<blk_hash_t>>> getLatestPivotAndTips() const;

  void getGhostPath(blk_hash_t const &source, std::vector<blk_hash_t> &ghost) const;
  void getGhostPath(std::vector<blk_hash_t> &ghost) const;  // get ghost path from last anchor
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
    SharedLock lock(mutex_);
    return period_;
  }
  std::pair<blk_hash_t, blk_hash_t> getAnchors() const {
    SharedLock lock(mutex_);
    return std::make_pair(old_anchor_, anchor_);
  }

  /**
   * @brief Retrieves Dag expiry limit
   *
   * @return limit
   */
  uint32_t getDagExpiryLimit() const { return dag_expiry_limit_; }

  const std::pair<uint64_t, std::map<uint64_t, std::unordered_set<blk_hash_t>>> getNonFinalizedBlocks() const;

  /**
   * @brief Retrieves current period together with non finalized blocks with the unique list of non finalized
   * transactions from these blocks
   *
   * @param known_hashes excludes known hashes from result
   * @return Period, blocks and transactions
   */
  const std::tuple<uint64_t, std::vector<std::shared_ptr<DagBlock>>, SharedTransactions>
  getNonFinalizedBlocksWithTransactions(const std::unordered_set<blk_hash_t> &known_hashes) const;

  DagFrontier getDagFrontier();

  /**
   * @return std::pair<size_t, size_t> -> first = number of levels, second = number of blocks
   */
  std::pair<size_t, size_t> getNonFinalizedBlocksSize() const;

  util::Event<DagManager, DagBlock> const block_verified_{};

  /**
   * @brief Retrieves Dag Manager mutex, only to be used when finalizing pbft block
   *
   * @return mutex
   */
  std::shared_mutex &getDagMutex() { return mutex_; }

 private:
  void recoverDag();
  void addToDag(blk_hash_t const &hash, blk_hash_t const &pivot, std::vector<blk_hash_t> const &tips, uint64_t level,
                bool finalized = false);
  bool validateBlockNotExpired(const std::shared_ptr<DagBlock> &dag_block,
                               std::unordered_map<blk_hash_t, std::shared_ptr<DagBlock>> &expired_dag_blocks_to_remove);
  void handleExpiredDagBlocksTransactions(const std::vector<trx_hash_t> &transactions_from_expired_dag_blocks) const;
  void clearLightNodeHistory();

  void worker();
  std::pair<blk_hash_t, std::vector<blk_hash_t>> getFrontier() const;  // return pivot and tips
  void updateFrontier();
  std::atomic<level_t> max_level_ = 0;
  mutable std::shared_mutex mutex_;
  mutable std::shared_mutex order_dag_blocks_mutex_;
  std::shared_ptr<PivotTree> pivot_tree_;  // only contains pivot edges
  std::shared_ptr<Dag> total_dag_;         // contains both pivot and tips
  std::shared_ptr<TransactionManager> trx_mgr_;
  std::shared_ptr<PbftChain> pbft_chain_;
  std::shared_ptr<DagBlockManager> dag_blk_mgr_;
  std::weak_ptr<Network> network_;
  std::shared_ptr<DbStorage> db_;
  blk_hash_t anchor_;      // anchor of the last period
  blk_hash_t old_anchor_;  // anchor of the second to last period
  uint64_t period_;        // last period
  blk_hash_t genesis_;
  std::map<uint64_t, std::unordered_set<blk_hash_t>> non_finalized_blks_;
  DagFrontier frontier_;
  std::atomic<bool> stopped_ = true;
  std::thread block_worker_;

  const bool is_light_node_ = false;
  const uint64_t light_node_history_ = 0;
  const uint32_t max_levels_per_period_;
  const uint32_t dag_expiry_limit_;  // Any non finalized dag block with a level smaller by
                                     // dag_expiry_limit_ than the current period anchor level is considered
                                     // expired and it should be ignored or removed from DAG

  logger::Logger log_time_;
  LOG_OBJECTS_DEFINE
};

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
