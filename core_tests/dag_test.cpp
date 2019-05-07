/*
 * @Copyright: Taraxa.io
 * @Author: Chia-Chun Lin
 * @Date: 2019-01-28 11:12:11
 * @Last Modified by: Chia-Chun Lin
 * @Last Modified time: 2019-03-16 23:33:41
 */

#include "dag.hpp"
#include <gtest/gtest.h>
#include "libdevcore/Log.h"
#include "types.hpp"

namespace taraxa {
TEST(Dag, build_dag) {
  taraxa::Dag graph;

  // a genesis vertex
  EXPECT_EQ(1, graph.getNumVertices());

  auto v1 = "0000000000000000000000000000000000000000000000000000000000000001";
  auto v2 = "0000000000000000000000000000000000000000000000000000000000000002";
  auto v3 = "0000000000000000000000000000000000000000000000000000000000000003";

  std::vector<std::string> empty;
  graph.addVEEs(v1, Dag::GENESIS, empty);
  EXPECT_EQ(2, graph.getNumVertices());
  EXPECT_EQ(1, graph.getNumEdges());

  // try insert same vertex, no multiple edges
  graph.addVEEs(v1, Dag::GENESIS, empty);
  EXPECT_EQ(2, graph.getNumVertices());
  EXPECT_EQ(1, graph.getNumEdges());

  graph.addVEEs(v2, Dag::GENESIS, empty);
  EXPECT_EQ(3, graph.getNumVertices());
  EXPECT_EQ(2, graph.getNumEdges());

  graph.addVEEs(v3, v1, {v2});
  EXPECT_EQ(4, graph.getNumVertices());
  EXPECT_EQ(4, graph.getNumEdges());
}

TEST(Dag, dag_traverse_get_children_tips) {
  taraxa::Dag graph;

  // a genesis vertex
  EXPECT_EQ(1, graph.getNumVertices());

  auto v1 = "0000000000000000000000000000000000000000000000000000000000000001";
  auto v2 = "0000000000000000000000000000000000000000000000000000000000000002";
  auto v3 = "0000000000000000000000000000000000000000000000000000000000000003";
  auto v4 = "0000000000000000000000000000000000000000000000000000000000000004";
  auto v5 = "0000000000000000000000000000000000000000000000000000000000000005";
  auto v6 = "0000000000000000000000000000000000000000000000000000000000000006";
  auto v7 = "0000000000000000000000000000000000000000000000000000000000000007";
  auto v8 = "0000000000000000000000000000000000000000000000000000000000000008";
  auto v9 = "0000000000000000000000000000000000000000000000000000000000000009";

  std::vector<std::string> empty;
  std::string no = "";
  // isolate node
  graph.addVEEs(v1, no, empty);
  graph.addVEEs(v2, no, empty);
  EXPECT_EQ(3, graph.getNumVertices());
  EXPECT_EQ(0, graph.getNumEdges());

  std::vector<std::string> leaves;
  std::string pivot;
  graph.getLeaves(leaves);
  EXPECT_EQ(3, leaves.size());

  graph.addVEEs(v3, Dag::GENESIS, empty);
  graph.addVEEs(v4, Dag::GENESIS, empty);
  EXPECT_EQ(5, graph.getNumVertices());
  EXPECT_EQ(2, graph.getNumEdges());

  graph.addVEEs(v5, v3, empty);
  graph.addVEEs(v6, v3, empty);
  graph.addVEEs(v7, v3, empty);
  EXPECT_EQ(8, graph.getNumVertices());
  EXPECT_EQ(5, graph.getNumEdges());

  graph.addVEEs(v8, v6, {v5});
  graph.addVEEs(v9, v6, empty);
  leaves.clear();
  graph.getLeaves(leaves);
  EXPECT_EQ(6, leaves.size());
  EXPECT_EQ(10, graph.getNumVertices());
  EXPECT_EQ(8, graph.getNumEdges());

#ifdef TARAXA_DEBUG

  time_stamp_t t4 = graph.getVertexTimeStamp(v4);
  time_stamp_t t4p1 = t4 + 1;
  time_stamp_t t5 = graph.getVertexTimeStamp(v5);
  time_stamp_t t5p1 = t5 + 1;
  time_stamp_t t6 = graph.getVertexTimeStamp(v6);
  time_stamp_t t6p1 = t6 + 1;
  time_stamp_t t7 = graph.getVertexTimeStamp(v7);
  time_stamp_t t7p1 = t7 + 1;
  time_stamp_t t8 = graph.getVertexTimeStamp(v8);
  time_stamp_t t8p1 = t8 + 1;
  EXPECT_NE(t4, t5);
  EXPECT_NE(t5, t6);
  EXPECT_NE(t6, t7);
  EXPECT_NE(t7, t8);
  {
    std::vector<std::string> children;
    graph.getChildrenBeforeTimeStamp(v3, t7, children);
    EXPECT_EQ(children.size(), 2);
  }
  {
    std::vector<std::string> children;
    graph.getChildrenBeforeTimeStamp(v6, t7, children);
    EXPECT_EQ(children.size(), 0);
  }
  {
    std::vector<std::string> tips;
    graph.getLeavesBeforeTimeStamp(v3, t7p1, tips);
    EXPECT_EQ(tips.size(), 3);
  }
  {
    std::vector<std::string> tips;
    graph.getLeavesBeforeTimeStamp(v3, t7, tips);
    EXPECT_EQ(tips.size(), 2);
  }
  {
    std::vector<std::string> tips;
    graph.getLeavesBeforeTimeStamp(v4, t7p1, tips);
    EXPECT_EQ(tips.size(), 1);
  }

  {
    std::vector<std::string> sub_tree;
    graph.getSubtreeBeforeTimeStamp(Dag::GENESIS, t8p1, sub_tree);
    EXPECT_EQ(sub_tree.size(), 6);
    EXPECT_EQ(sub_tree.back(), v8);
  }

  {
    std::vector<std::string> sub_tree;
    graph.getSubtreeBeforeTimeStamp(v3, t7, sub_tree);
    EXPECT_EQ(sub_tree.size(), 2);
    EXPECT_EQ(sub_tree.back(), v6);
  }
#endif
}

TEST(Dag, dag_traverse2_get_children_tips) {
  taraxa::Dag graph;

  // a genesis vertex
  EXPECT_EQ(1, graph.getNumVertices());

  auto v1 = "0000000000000000000000000000000000000000000000000000000000000001";
  auto v2 = "0000000000000000000000000000000000000000000000000000000000000002";
  auto v3 = "0000000000000000000000000000000000000000000000000000000000000003";
  auto v4 = "0000000000000000000000000000000000000000000000000000000000000004";
  auto v5 = "0000000000000000000000000000000000000000000000000000000000000005";
  auto v6 = "0000000000000000000000000000000000000000000000000000000000000006";

  std::vector<std::string> empty;
  std::string no = "";

  graph.addVEEs(v1, Dag::GENESIS, empty);
  graph.addVEEs(v2, v1, empty);
  graph.addVEEs(v3, v2, empty);
  graph.addVEEs(v4, v2, empty);
  graph.addVEEs(v5, v2, empty);
  graph.addVEEs(v6, v4, {v5});

  EXPECT_EQ(7, graph.getNumVertices());
  EXPECT_EQ(7, graph.getNumEdges());

#ifdef TARAXA_DEBUG
  time_stamp_t t4 = graph.getVertexTimeStamp(v4);
  time_stamp_t t5 = graph.getVertexTimeStamp(v5);
  time_stamp_t t5p1 = t5 + 1;
  time_stamp_t t6 = graph.getVertexTimeStamp(v6);
  time_stamp_t t6p1 = t6 + 1;
  std::vector<std::string> children, tips;
  graph.getChildrenBeforeTimeStamp(v2, t5, children);
  EXPECT_EQ(children.size(), 2);

  graph.getChildrenBeforeTimeStamp(v2, t5p1, children);
  EXPECT_EQ(children.size(), 3);

  graph.getLeavesBeforeTimeStamp(v4, t6p1, tips);
  EXPECT_EQ(tips.size(), 1);

  graph.getLeavesBeforeTimeStamp(v1, t5, tips);
  EXPECT_EQ(tips.size(), 2);

  graph.getLeavesBeforeTimeStamp(v1, t6p1, tips);
  EXPECT_EQ(tips.size(), 2);

  // if no valid children, return self
  graph.getLeavesBeforeTimeStamp(v4, t5, tips);
  EXPECT_EQ(tips.size(), 1);

  graph.getLeavesBeforeTimeStamp(v4, t4, tips);
  EXPECT_EQ(tips.size(), 0);

  time_stamp_t stamp = 100;
  graph.setVertexTimeStamp(v1, stamp);
  EXPECT_EQ(graph.getVertexTimeStamp(v1), stamp);

  graph.setVertexTimeStamp(Dag::GENESIS, stamp);
  EXPECT_EQ(graph.getVertexTimeStamp(Dag::GENESIS), stamp);
#endif
}

// Use the example on Conflux paper
TEST(Dag, dag_traverse3_get_epfriend) {
  taraxa::Dag graph;
  auto vA = "0000000000000000000000000000000000000000000000000000000000000001";
  auto vB = "0000000000000000000000000000000000000000000000000000000000000002";
  auto vC = "0000000000000000000000000000000000000000000000000000000000000003";
  auto vD = "0000000000000000000000000000000000000000000000000000000000000004";
  auto vE = "0000000000000000000000000000000000000000000000000000000000000005";
  auto vF = "0000000000000000000000000000000000000000000000000000000000000006";
  auto vG = "0000000000000000000000000000000000000000000000000000000000000007";
  auto vH = "0000000000000000000000000000000000000000000000000000000000000008";
  auto vI = "0000000000000000000000000000000000000000000000000000000000000009";
  auto vJ = "000000000000000000000000000000000000000000000000000000000000000A";
  auto vK = "000000000000000000000000000000000000000000000000000000000000000B";

  std::vector<std::string> empty;
  std::string no = "";
  graph.addVEEs(vA, Dag::GENESIS, empty);
  graph.addVEEs(vB, Dag::GENESIS, empty);
  graph.addVEEs(vC, vA, {vB});
  graph.addVEEs(vD, vA, empty);
  graph.addVEEs(vF, vB, empty);
  graph.addVEEs(vE, vC, {vD, vF});
  graph.addVEEs(vG, vA, empty);
  graph.addVEEs(vJ, vF, empty);
  graph.addVEEs(vI, vJ, empty);
  graph.addVEEs(vH, vE, {vG, vI});
  graph.addVEEs(vK, vI, empty);

  std::vector<std::string> epfriend;
  std::unordered_set<std::string> recent_added_blks;
  // read only
  graph.getEpFriendVertices(vE, vH, epfriend);
  EXPECT_EQ(epfriend.size(), 4);

  // ------------------ epoch A ------------------
  recent_added_blks.insert(vA);
  graph.updatePeriodVerticesAndComputeOrder(Dag::GENESIS, vA, 1,
                                            recent_added_blks, epfriend);

  EXPECT_EQ(epfriend.size(), 1);  // vA
  EXPECT_EQ(recent_added_blks.size(), 0);

  // ------------------ epoch C ------------------

  recent_added_blks.insert(vB);
  recent_added_blks.insert(vC);

  graph.updatePeriodVerticesAndComputeOrder(vA, vC, 2, recent_added_blks,
                                            epfriend);
  EXPECT_EQ(epfriend.size(), 2);  // vB, vC
  EXPECT_EQ(recent_added_blks.size(), 0);

  // ------------------ epoch E ------------------

  recent_added_blks.insert(vD);
  recent_added_blks.insert(vE);
  recent_added_blks.insert(vF);
  graph.updatePeriodVerticesAndComputeOrder(vC, vE, 3, recent_added_blks,
                                            epfriend);
  EXPECT_EQ(epfriend.size(), 3);  // vD, vF, vE
  EXPECT_EQ(recent_added_blks.size(), 0);

  // ------------------ epoch H ------------------
  recent_added_blks.insert(vG);
  recent_added_blks.insert(vH);
  recent_added_blks.insert(vI);
  recent_added_blks.insert(vJ);
  graph.updatePeriodVerticesAndComputeOrder(vE, vH, 4, recent_added_blks,
                                            epfriend);
  EXPECT_EQ(epfriend.size(), 4);  // vG, vJ, vI, vH
  EXPECT_EQ(recent_added_blks.size(), 0);

  if (epfriend.size() == 4) {
    EXPECT_EQ(epfriend[0], vJ);
    EXPECT_EQ(epfriend[1], vI);
    EXPECT_EQ(epfriend[2], vG);
    EXPECT_EQ(epfriend[3], vH);
  }
}
TEST(PivotTree, genesis_get_pivot) {
  taraxa::PivotTree graph;

  std::vector<std::string> pivot_chain, leaves;
  graph.getGhostPath(Dag::GENESIS, pivot_chain);
  EXPECT_EQ(pivot_chain.size(), 1);
  graph.getLeaves(leaves);
  EXPECT_EQ(leaves.size(), 1);
}

TEST(PivotTree, dag_traverse_pivot_chain_and_subtree) {
  taraxa::PivotTree graph;

  auto v1 = "0000000000000000000000000000000000000000000000000000000000000001";
  auto v2 = "0000000000000000000000000000000000000000000000000000000000000002";
  auto v3 = "0000000000000000000000000000000000000000000000000000000000000003";
  auto v4 = "0000000000000000000000000000000000000000000000000000000000000004";
  auto v5 = "0000000000000000000000000000000000000000000000000000000000000005";
  auto v6 = "0000000000000000000000000000000000000000000000000000000000000006";
  auto v7 = "0000000000000000000000000000000000000000000000000000000000000007";
  auto v8 = "0000000000000000000000000000000000000000000000000000000000000008";
  auto v9 = "0000000000000000000000000000000000000000000000000000000000000009";
  auto v10 = "000000000000000000000000000000000000000000000000000000000000000A";
  auto v11 = "000000000000000000000000000000000000000000000000000000000000000B";
  std::vector<std::string> empty;
  std::string no = "";
  graph.addVEEs(v1, Dag::GENESIS, empty);
  graph.addVEEs(v2, Dag::GENESIS, empty);
  graph.addVEEs(v3, Dag::GENESIS, empty);
  graph.addVEEs(v4, v1, empty);
  graph.addVEEs(v5, v1, empty);
  graph.addVEEs(v6, v2, empty);
  graph.addVEEs(v7, v2, empty);
  graph.addVEEs(v8, v3, empty);
  graph.addVEEs(v9, v7, empty);
  graph.addVEEs(v10, v9, empty);
  graph.addVEEs(v11, v9, empty);

  EXPECT_EQ(12, graph.getNumVertices());
  EXPECT_EQ(11, graph.getNumEdges());

#ifdef TARAXA_DEBUG

  time_stamp_t t9 = graph.getVertexTimeStamp(v9);
  time_stamp_t t9p1 = t9 + 1;
  time_stamp_t t10 = graph.getVertexTimeStamp(v10);

  time_stamp_t t11 = graph.getVertexTimeStamp(v11);
  time_stamp_t t11p1 = t11 + 1;
  EXPECT_NE(t9, t10);
  EXPECT_NE(t10, t11);

  // timestamp exclude v9
  {
    std::vector<std::string> pivot_chain;
    graph.getGhostPath(Dag::GENESIS, pivot_chain);
    EXPECT_EQ(pivot_chain.size(), 5);
    EXPECT_EQ(pivot_chain.back(), v10);
  }

  {
    std::vector<std::string> pivot_chain;
    graph.getGhostPathBeforeTimeStamp(Dag::GENESIS, t11p1, pivot_chain);
    EXPECT_EQ(pivot_chain.size(), 5);
    EXPECT_EQ(pivot_chain.back(), v10);
  }

  {
    std::vector<std::string> pivot_chain;
    graph.getGhostPathBeforeTimeStamp(Dag::GENESIS, t9, pivot_chain);
    EXPECT_EQ(pivot_chain.size(), 3);
    EXPECT_EQ(pivot_chain.back(), v4);
  }
#endif
}

TEST(DagManager, dag_traverse_pivot_chain_and_subtree_2) {
  taraxa::PivotTree graph;

  auto v1 = "0000000000000000000000000000000000000000000000000000000000000001";
  auto v2 = "0000000000000000000000000000000000000000000000000000000000000002";

  std::vector<std::string> empty;
  std::string no = "";

  graph.addVEEs(v1, Dag::GENESIS, empty);
  graph.addVEEs(v2, Dag::GENESIS, empty);
  graph.setVertexTimeStamp(Dag::GENESIS, 1);
  graph.setVertexTimeStamp(v1, 50);
  graph.setVertexTimeStamp(v2, 25);

  {
    std::vector<std::string> pivot_chain;
    graph.getGhostPathBeforeTimeStamp(Dag::GENESIS, 26, pivot_chain);
    EXPECT_EQ(pivot_chain.size(), 2);
    EXPECT_EQ(pivot_chain.back(), v2);
    graph.getGhostPathBeforeTimeStamp(Dag::GENESIS, 51, pivot_chain);
    EXPECT_EQ(pivot_chain.size(), 2);
    EXPECT_EQ(pivot_chain.back(), v1);
  }
}

// Use the example on Conflux paper
TEST(DagManager, compute_epoch) {
  auto mgr = std::make_shared<DagManager>(1);
  mgr->start();
  DagBlock blkA(blk_hash_t(0), {}, {trx_hash_t(2)}, sig_t(1), blk_hash_t(1),
                addr_t(1));
  DagBlock blkB(blk_hash_t(0), {}, {trx_hash_t(3), trx_hash_t(4)}, sig_t(1),
                blk_hash_t(2), addr_t(1));
  DagBlock blkC(blk_hash_t(1), {blk_hash_t(2)}, {}, sig_t(1), blk_hash_t(3),
                addr_t(1));
  DagBlock blkD(blk_hash_t(1), {}, {}, sig_t(1), blk_hash_t(4), addr_t(1));
  DagBlock blkE(blk_hash_t(3), {blk_hash_t(4), blk_hash_t(6)}, {}, sig_t(1),
                blk_hash_t(5), addr_t(1));
  DagBlock blkF(blk_hash_t(2), {}, {}, sig_t(1), blk_hash_t(6), addr_t(1));
  DagBlock blkG(blk_hash_t(1), {}, {trx_hash_t(4)}, sig_t(1), blk_hash_t(7),
                addr_t(1));
  DagBlock blkH(blk_hash_t(5), {blk_hash_t(7), blk_hash_t(9)}, {}, sig_t(1),
                blk_hash_t(8), addr_t(1));
  DagBlock blkI(blk_hash_t(10), {blk_hash_t(3)}, {}, sig_t(1), blk_hash_t(9),
                addr_t(1));
  DagBlock blkJ(blk_hash_t(6), {}, {}, sig_t(1), blk_hash_t(10), addr_t(1));
  DagBlock blkK(blk_hash_t(9), {}, {}, sig_t(1), blk_hash_t(11), addr_t(1));
  mgr->addDagBlock(blkA, true);
  mgr->addDagBlock(blkB, true);
  mgr->addDagBlock(blkC, true);
  mgr->addDagBlock(blkD, true);
  mgr->addDagBlock(blkE, true);
  taraxa::thisThreadSleepForMilliSeconds(100);
  mgr->addDagBlock(blkF, true);
  mgr->addDagBlock(blkG, true);
  mgr->addDagBlock(blkH, true);
  mgr->addDagBlock(blkI, true);
  mgr->addDagBlock(blkJ, true);
  mgr->addDagBlock(blkK, true);
  taraxa::thisThreadSleepForMilliSeconds(100);

  vec_blk_t orders;
  uint64_t period;
  period = mgr->createPeriodAndComputeBlockOrder(blkA.getHash(), orders);
  EXPECT_EQ(orders.size(), 1);
  EXPECT_EQ(period, 1);
  period = mgr->createPeriodAndComputeBlockOrder(blkC.getHash(), orders);
  EXPECT_EQ(orders.size(), 2);
  EXPECT_EQ(period, 2);
  period = mgr->createPeriodAndComputeBlockOrder(blkE.getHash(), orders);
  EXPECT_EQ(orders.size(), 3);
  EXPECT_EQ(period, 3);
  period = mgr->createPeriodAndComputeBlockOrder(blkH.getHash(), orders);
  EXPECT_EQ(orders.size(), 4);
  EXPECT_EQ(period, 4);
  if (orders.size() == 4) {
    EXPECT_EQ(orders[0], blk_hash_t(10));
    EXPECT_EQ(orders[1], blk_hash_t(9));
    EXPECT_EQ(orders[2], blk_hash_t(7));
    EXPECT_EQ(orders[3], blk_hash_t(8));
  }
  period = mgr->createPeriodAndComputeBlockOrder(blkK.getHash(), orders);
  EXPECT_EQ(orders.size(), 1);
  EXPECT_EQ(period, 5);
}

TEST(DagManager, receive_block_in_order) {
  auto mgr = std::make_shared<DagManager>(1);
  mgr->start();
  // mgr.setVerbose(true);
  DagBlock blk1(blk_hash_t(0), {}, {}, sig_t(777), blk_hash_t(1), addr_t(15));
  DagBlock blk2(blk_hash_t(1), {}, {}, sig_t(777), blk_hash_t(2), addr_t(15));
  DagBlock blk3(blk_hash_t(0), {blk_hash_t(1), blk_hash_t(2)}, {}, sig_t(777),
                blk_hash_t(3), addr_t(15));

  mgr->addDagBlock(blk1, true);
  mgr->addDagBlock(blk2, true);
  mgr->addDagBlock(blk2, true);
  EXPECT_EQ(mgr->getNumVerticesInDag().first, 3);
  EXPECT_EQ(mgr->getNumEdgesInDag().first, 2);

  mgr->addDagBlock(blk3, true);
  mgr->addDagBlock(blk3, true);
  taraxa::thisThreadSleepForMilliSeconds(500);

  std::string pivot;
  std::vector<std::string> tips;
  std::vector<Dag::vertex_t> criticals;
  mgr->getLatestPivotAndTips(pivot, tips);

  EXPECT_EQ(pivot,
            "0000000000000000000000000000000000000000000000000000000000000002");
  EXPECT_EQ(tips.size(), 1);
  mgr->stop();
  EXPECT_EQ(mgr->getNumVerticesInDag().first, 4);
  // total edges
  EXPECT_EQ(mgr->getNumEdgesInDag().first, 5);
  // pivot edges
  EXPECT_EQ(mgr->getNumEdgesInDag().second, 3);

  EXPECT_EQ(mgr->getBufferSize(), 0);
}

TEST(DagManager, receive_block_out_of_order) {
  auto mgr = std::make_shared<DagManager>(1);
  mgr->start();

  // mgr.setVerbose(true);
  DagBlock blk1(blk_hash_t(0), {}, {}, sig_t(777), blk_hash_t(1), addr_t(15));
  DagBlock blk2(blk_hash_t(1), {}, {}, sig_t(777), blk_hash_t(2), addr_t(15));
  DagBlock blk3(blk_hash_t(0), {blk_hash_t(1), blk_hash_t(2)}, {}, sig_t(777),
                blk_hash_t(3), addr_t(15));

  mgr->addDagBlock(blk3, true);
  mgr->addDagBlock(blk2, true);
  mgr->addDagBlock(blk1, true);
  taraxa::thisThreadSleepForMicroSeconds(500);

  std::string pivot;
  std::vector<std::string> tips;
  std::vector<Dag::vertex_t> criticals;
  mgr->getLatestPivotAndTips(pivot, tips);

  EXPECT_EQ(pivot,
            "0000000000000000000000000000000000000000000000000000000000000002");
  EXPECT_EQ(tips.size(), 1);
  mgr->stop();
  EXPECT_EQ(mgr->getNumVerticesInDag().first, 4);
  EXPECT_EQ(mgr->getNumEdgesInDag().first, 5);
  EXPECT_EQ(mgr->getBufferSize(), 0);
}

TEST(DagManager, get_latest_pivot_tips) {
  auto mgr = std::make_shared<DagManager>(1);
  mgr->start();

  // mgr.setVerbose(true);
  DagBlock blk1(blk_hash_t(0), {}, {}, sig_t(0), blk_hash_t(1), addr_t(15));
  DagBlock blk2(blk_hash_t(1), {}, {}, sig_t(1), blk_hash_t(2), addr_t(15));
  DagBlock blk3(blk_hash_t(2), {}, {}, sig_t(1), blk_hash_t(3), addr_t(15));
  DagBlock blk4(blk_hash_t(1), {}, {}, sig_t(1), blk_hash_t(4), addr_t(15));
  DagBlock blk5(blk_hash_t(4), {}, {}, sig_t(1), blk_hash_t(5), addr_t(15));
  DagBlock blk6(blk_hash_t(2), {blk_hash_t(5)}, {}, sig_t(1), blk_hash_t(6),
                addr_t(15));
  mgr->addDagBlock(blk3, true);
  mgr->addDagBlock(blk6, true);
  mgr->addDagBlock(blk4, true);
  mgr->addDagBlock(blk5, true);
  mgr->addDagBlock(blk2, true);
  mgr->addDagBlock(blk1, true);
  taraxa::thisThreadSleepForMilliSeconds(100);

  std::string pivot;
  std::vector<std::string> tips;
  std::vector<Dag::vertex_t> criticals;
  mgr->getLatestPivotAndTips(pivot, tips);

  EXPECT_EQ(pivot,
            "0000000000000000000000000000000000000000000000000000000000000003");
  EXPECT_EQ(tips.size(), 1);
  EXPECT_EQ(tips[0],
            "0000000000000000000000000000000000000000000000000000000000000006");
  mgr->stop();
}
/**
 * Note: TODO, Disable for now
 * The first thread has more change to win the Dag lock,
 * probably need to add some variation
 */

/*
TEST(DagManager, receive_block_out_of_order_multi_thread){
       auto mgr = std::make_shared<DagManager> (2);

       //mgr->setVerbose(true);
       mgr->start();

       DagBlock blk1 (
       ("0000000000000000000000000000000000000000000000000000000000000000"),
       {},
       {},
       ("77777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777"),
       ("0000000000000000000000000000000000000000000000000000000000000001"));

       DagBlock blk2 (
       ("0000000000000000000000000000000000000000000000000000000000000001"),
       {},
       {},
       ("77777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777"),
       ("0000000000000000000000000000000000000000000000000000000000000002"));

       DagBlock blk3 (
       ("0000000000000000000000000000000000000000000000000000000000000000"),
       {"0000000000000000000000000000000000000000000000000000000000000001",
        "0000000000000000000000000000000000000000000000000000000000000002"
       },
       {},
       ("77777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777"),
       ("0000000000000000000000000000000000000000000000000000000000000003"));

       DagBlock blk4 (
       ("0000000000000000000000000000000000000000000000000000000000000002"),
       {"0000000000000000000000000000000000000000000000000000000000000003",
       },
       {},
       ("77777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777"),
       ("0000000000000000000000000000000000000000000000000000000000000004"));

       DagBlock blk5 (
       ("0000000000000000000000000000000000000000000000000000000000000004"),
       {"0000000000000000000000000000000000000000000000000000000000000003",
       },
       {},
       ("77777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777"),
       ("0000000000000000000000000000000000000000000000000000000000000005"));

       DagBlock blk6 (
       ("0000000000000000000000000000000000000000000000000000000000000005"),
       {},
       {},
       ("77777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777"),
       ("0000000000000000000000000000000000000000000000000000000000000006"));

       mgr->addDagBlock(blk6, true);
       mgr->addDagBlock(blk5, true);
       mgr->addDagBlock(blk4, true);
       mgr->addDagBlock(blk3, true);
       mgr->addDagBlock(blk2, true);
       mgr->addDagBlock(blk1, true);


       thisThreadSleepForMicroSeconds(500);
       mgr->stop();
       EXPECT_EQ(mgr->getNumVerticesInDag(),7);
       EXPECT_EQ(mgr->getNumEdgesInDag(),10);
       EXPECT_EQ(mgr->getBufferSize(), 0);

}
*/

}  // namespace taraxa

int main(int argc, char** argv) {
  dev::LoggingOptions logOptions;
  logOptions.verbosity = dev::VerbosityWarning;
  dev::setupLogging(logOptions);
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}