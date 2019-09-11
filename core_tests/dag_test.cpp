#include "dag.hpp"
#include <gtest/gtest.h>
#include "libdevcore/Log.h"
#include "types.hpp"

namespace taraxa {
TEST(Dag, build_dag) {
  const std::string GENESIS =
      "0000000000000000000000000000000000000000000000000000000000000000";
  taraxa::Dag graph(GENESIS);

  // a genesis vertex
  EXPECT_EQ(1, graph.getNumVertices());

  auto v1 = "0000000000000000000000000000000000000000000000000000000000000001";
  auto v2 = "0000000000000000000000000000000000000000000000000000000000000002";
  auto v3 = "0000000000000000000000000000000000000000000000000000000000000003";

  std::vector<std::string> empty;
  graph.addVEEs(v1, GENESIS, empty);
  EXPECT_EQ(2, graph.getNumVertices());
  EXPECT_EQ(1, graph.getNumEdges());

  // try insert same vertex, no multiple edges
  graph.addVEEs(v1, GENESIS, empty);
  EXPECT_EQ(2, graph.getNumVertices());
  EXPECT_EQ(1, graph.getNumEdges());

  graph.addVEEs(v2, GENESIS, empty);
  EXPECT_EQ(3, graph.getNumVertices());
  EXPECT_EQ(2, graph.getNumEdges());

  graph.addVEEs(v3, v1, {v2});
  EXPECT_EQ(4, graph.getNumVertices());
  EXPECT_EQ(4, graph.getNumEdges());
}

TEST(Dag, dag_traverse_get_children_tips) {
  const std::string GENESIS =
      "0000000000000000000000000000000000000000000000000000000000000000";
  taraxa::Dag graph(GENESIS);

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

  graph.addVEEs(v3, GENESIS, empty);
  graph.addVEEs(v4, GENESIS, empty);
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
}

TEST(Dag, dag_traverse2_get_children_tips) {
  const std::string GENESIS =
      "0000000000000000000000000000000000000000000000000000000000000000";
  taraxa::Dag graph(GENESIS);

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

  graph.addVEEs(v1, GENESIS, empty);
  graph.addVEEs(v2, v1, empty);
  graph.addVEEs(v3, v2, empty);
  graph.addVEEs(v4, v2, empty);
  graph.addVEEs(v5, v2, empty);
  graph.addVEEs(v6, v4, {v5});

  EXPECT_EQ(7, graph.getNumVertices());
  EXPECT_EQ(7, graph.getNumEdges());
}

// Use the example on Conflux paper
TEST(Dag, dag_traverse3_get_ordered_blks) {
  const std::string GENESIS =
      "0000000000000000000000000000000000000000000000000000000000000000";
  taraxa::Dag graph(GENESIS);
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
  graph.addVEEs(vA, GENESIS, empty);
  graph.addVEEs(vB, GENESIS, empty);
  graph.addVEEs(vC, vA, {vB});
  graph.addVEEs(vD, vA, empty);
  graph.addVEEs(vF, vB, empty);
  graph.addVEEs(vE, vC, {vD, vF});
  graph.addVEEs(vG, vA, empty);
  graph.addVEEs(vJ, vF, empty);
  graph.addVEEs(vI, vJ, empty);
  graph.addVEEs(vH, vE, {vG, vI});
  graph.addVEEs(vK, vI, empty);

  std::vector<std::string> ordered_blks;
  std::unordered_set<std::string> recent_added_blks;
  // read only
  graph.getEpFriendVertices(vE, vH, ordered_blks);
  EXPECT_EQ(ordered_blks.size(), 4);

  // ------------------ epoch A ------------------

  recent_added_blks.insert(vA);

  {  // get only, do not finalize
    graph.computeOrder(false /*finialized */, vA, 1, recent_added_blks,
                       ordered_blks);

    EXPECT_EQ(ordered_blks.size(), 1);  // vA
    EXPECT_EQ(recent_added_blks.size(), 1);
  }
  graph.computeOrder(true /*finialized */, vA, 1, recent_added_blks,
                     ordered_blks);

  EXPECT_EQ(ordered_blks.size(), 1);  // vA
  EXPECT_EQ(recent_added_blks.size(), 0);

  // ------------------ epoch C ------------------

  recent_added_blks.insert(vB);
  recent_added_blks.insert(vC);

  {  // get only, do not finalize
    graph.computeOrder(false /*finialized */, vC, 2, recent_added_blks,
                       ordered_blks);
    EXPECT_EQ(ordered_blks.size(), 2);  // vB, vC
    EXPECT_EQ(recent_added_blks.size(), 2);
  }

  graph.computeOrder(true /*finialized */, vC, 2, recent_added_blks,
                     ordered_blks);
  EXPECT_EQ(ordered_blks.size(), 2);  // vB, vC
  EXPECT_EQ(recent_added_blks.size(), 0);

  // ------------------ epoch E ------------------

  recent_added_blks.insert(vD);
  recent_added_blks.insert(vE);
  recent_added_blks.insert(vF);
  graph.computeOrder(true /*finialized */, vE, 3, recent_added_blks,
                     ordered_blks);
  EXPECT_EQ(ordered_blks.size(), 3);  // vD, vF, vE
  EXPECT_EQ(recent_added_blks.size(), 0);

  // ------------------ epoch H ------------------
  recent_added_blks.insert(vG);
  recent_added_blks.insert(vH);
  recent_added_blks.insert(vI);
  recent_added_blks.insert(vJ);
  graph.computeOrder(true /*finialized */, vH, 4, recent_added_blks,
                     ordered_blks);
  EXPECT_EQ(ordered_blks.size(), 4);  // vG, vJ, vI, vH
  EXPECT_EQ(recent_added_blks.size(), 0);

  if (ordered_blks.size() == 4) {
    EXPECT_EQ(ordered_blks[0], vJ);
    EXPECT_EQ(ordered_blks[1], vI);
    EXPECT_EQ(ordered_blks[2], vG);
    EXPECT_EQ(ordered_blks[3], vH);
  }

  // ------------------ epoch H ------------------
  recent_added_blks.insert(vK);
  graph.computeOrder(true /*finialized */, vK, 5, recent_added_blks,
                     ordered_blks);
  EXPECT_EQ(ordered_blks.size(), 1);  // vK
  EXPECT_EQ(recent_added_blks.size(), 0);
  EXPECT_EQ(graph.getNumVertices(), 12);

  graph.deletePeriod(0);  // should be no op
  graph.deletePeriod(1);
  EXPECT_EQ(graph.getNumVertices(), 11);
  graph.deletePeriod(2);
  EXPECT_EQ(graph.getNumVertices(), 9);
  graph.deletePeriod(4);
  EXPECT_EQ(graph.getNumVertices(), 5);
  graph.deletePeriod(3);
  EXPECT_EQ(graph.getNumVertices(), 2);
  graph.deletePeriod(5);  // should be no op
  EXPECT_EQ(graph.getNumVertices(), 1);
}
TEST(PivotTree, genesis_get_pivot) {
  const std::string GENESIS =
      "0000000000000000000000000000000000000000000000000000000000000000";
  taraxa::PivotTree graph(GENESIS);

  std::vector<std::string> pivot_chain, leaves;
  graph.getGhostPath(GENESIS, pivot_chain);
  EXPECT_EQ(pivot_chain.size(), 1);
  graph.getLeaves(leaves);
  EXPECT_EQ(leaves.size(), 1);
}

// Use the example on Conflux paper
TEST(DagManager, compute_epoch) {
  const std::string GENESIS =
      "0000000000000000000000000000000000000000000000000000000000000000";
  auto mgr = std::make_shared<DagManager>(GENESIS);
  mgr->start();
  DagBlock blkA(blk_hash_t(0), 0, {}, {trx_hash_t(2)}, sig_t(1), blk_hash_t(1),
                addr_t(1));
  DagBlock blkB(blk_hash_t(0), 0, {}, {trx_hash_t(3), trx_hash_t(4)}, sig_t(1),
                blk_hash_t(2), addr_t(1));
  DagBlock blkC(blk_hash_t(1), 0, {blk_hash_t(2)}, {}, sig_t(1), blk_hash_t(3),
                addr_t(1));
  DagBlock blkD(blk_hash_t(1), 0, {}, {}, sig_t(1), blk_hash_t(4), addr_t(1));
  DagBlock blkE(blk_hash_t(3), 0, {blk_hash_t(4), blk_hash_t(6)}, {}, sig_t(1),
                blk_hash_t(5), addr_t(1));
  DagBlock blkF(blk_hash_t(2), 0, {}, {}, sig_t(1), blk_hash_t(6), addr_t(1));
  DagBlock blkG(blk_hash_t(1), 0, {}, {trx_hash_t(4)}, sig_t(1), blk_hash_t(7),
                addr_t(1));
  DagBlock blkH(blk_hash_t(5), 0, {blk_hash_t(7), blk_hash_t(9)}, {}, sig_t(1),
                blk_hash_t(8), addr_t(1));
  DagBlock blkI(blk_hash_t(10), 0, {blk_hash_t(3)}, {}, sig_t(1), blk_hash_t(9),
                addr_t(1));
  DagBlock blkJ(blk_hash_t(6), 0, {}, {}, sig_t(1), blk_hash_t(10), addr_t(1));
  DagBlock blkK(blk_hash_t(8), 0, {}, {}, sig_t(1), blk_hash_t(11), addr_t(1));
  mgr->addDagBlock(blkA);
  mgr->addDagBlock(blkB);
  mgr->addDagBlock(blkC);
  mgr->addDagBlock(blkD);
  mgr->addDagBlock(blkE);
  taraxa::thisThreadSleepForMilliSeconds(100);
  mgr->addDagBlock(blkF);
  mgr->addDagBlock(blkG);
  mgr->addDagBlock(blkH);
  mgr->addDagBlock(blkI);
  mgr->addDagBlock(blkJ);
  mgr->addDagBlock(blkK);
  taraxa::thisThreadSleepForMilliSeconds(100);

  vec_blk_t orders;
  uint64_t period;
  period = mgr->getDagBlockOrder(blkA.getHash(), orders);
  EXPECT_EQ(orders.size(), 1);
  EXPECT_EQ(period, 1);
  // repeat, should not change
  period = mgr->getDagBlockOrder(blkA.getHash(), orders);
  EXPECT_EQ(orders.size(), 1);
  EXPECT_EQ(period, 1);

  mgr->setDagBlockPeriod(blkA.getHash(), period);

  period = mgr->getDagBlockOrder(blkC.getHash(), orders);
  EXPECT_EQ(orders.size(), 2);
  EXPECT_EQ(period, 2);
  // repeat, should not change
  period = mgr->getDagBlockOrder(blkC.getHash(), orders);
  EXPECT_EQ(orders.size(), 2);
  EXPECT_EQ(period, 2);

  mgr->setDagBlockPeriod(blkC.getHash(), period);

  period = mgr->getDagBlockOrder(blkE.getHash(), orders);
  EXPECT_EQ(orders.size(), 3);
  EXPECT_EQ(period, 3);
  mgr->setDagBlockPeriod(blkE.getHash(), period);

  period = mgr->getDagBlockOrder(blkH.getHash(), orders);
  EXPECT_EQ(orders.size(), 4);
  EXPECT_EQ(period, 4);
  mgr->setDagBlockPeriod(blkH.getHash(), period);

  if (orders.size() == 4) {
    EXPECT_EQ(orders[0], blk_hash_t(10));
    EXPECT_EQ(orders[1], blk_hash_t(9));
    EXPECT_EQ(orders[2], blk_hash_t(7));
    EXPECT_EQ(orders[3], blk_hash_t(8));
  }
  period = mgr->getDagBlockOrder(blkK.getHash(), orders);
  EXPECT_EQ(orders.size(), 1);
  EXPECT_EQ(period, 5);
  mgr->setDagBlockPeriod(blkK.getHash(), period);
}

TEST(DagManager, receive_block_in_order) {
  const std::string GENESIS =
      "0000000000000000000000000000000000000000000000000000000000000000";
  auto mgr = std::make_shared<DagManager>(GENESIS);
  mgr->start();
  // mgr.setVerbose(true);
  DagBlock blk1(blk_hash_t(0), 0, {}, {}, sig_t(777), blk_hash_t(1),
                addr_t(15));
  DagBlock blk2(blk_hash_t(1), 0, {}, {}, sig_t(777), blk_hash_t(2),
                addr_t(15));
  DagBlock blk3(blk_hash_t(0), 0, {blk_hash_t(1), blk_hash_t(2)}, {},
                sig_t(777), blk_hash_t(3), addr_t(15));

  mgr->addDagBlock(blk1);
  mgr->addDagBlock(blk2);
  mgr->addDagBlock(blk2);
  EXPECT_EQ(mgr->getNumVerticesInDag().first, 3);
  EXPECT_EQ(mgr->getNumEdgesInDag().first, 2);

  mgr->addDagBlock(blk3);
  mgr->addDagBlock(blk3);
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

// Use the example on Conflux paper, insert block in different order and make
// sure block order are the same
TEST(DagManager, compute_epoch_2) {
  const std::string GENESIS =
      "0000000000000000000000000000000000000000000000000000000000000000";
  auto mgr = std::make_shared<DagManager>(GENESIS);
  mgr->start();
  DagBlock blkA(blk_hash_t(0), 0, {}, {trx_hash_t(2)}, sig_t(1), blk_hash_t(1),
                addr_t(1));
  DagBlock blkB(blk_hash_t(0), 0, {}, {trx_hash_t(3), trx_hash_t(4)}, sig_t(1),
                blk_hash_t(2), addr_t(1));
  DagBlock blkC(blk_hash_t(1), 0, {blk_hash_t(2)}, {}, sig_t(1), blk_hash_t(3),
                addr_t(1));
  DagBlock blkD(blk_hash_t(1), 0, {}, {}, sig_t(1), blk_hash_t(4), addr_t(1));
  DagBlock blkE(blk_hash_t(3), 0, {blk_hash_t(4), blk_hash_t(6)}, {}, sig_t(1),
                blk_hash_t(5), addr_t(1));
  DagBlock blkF(blk_hash_t(2), 0, {}, {}, sig_t(1), blk_hash_t(6), addr_t(1));
  DagBlock blkG(blk_hash_t(1), 0, {}, {trx_hash_t(4)}, sig_t(1), blk_hash_t(7),
                addr_t(1));
  DagBlock blkH(blk_hash_t(5), 0, {blk_hash_t(7), blk_hash_t(9)}, {}, sig_t(1),
                blk_hash_t(8), addr_t(1));
  DagBlock blkI(blk_hash_t(10), 0, {blk_hash_t(3)}, {}, sig_t(1), blk_hash_t(9),
                addr_t(1));
  DagBlock blkJ(blk_hash_t(6), 0, {}, {}, sig_t(1), blk_hash_t(10), addr_t(1));
  DagBlock blkK(blk_hash_t(9), 0, {}, {}, sig_t(1), blk_hash_t(11), addr_t(1));
  mgr->addDagBlock(blkA);
  mgr->addDagBlock(blkC);
  mgr->addDagBlock(blkB);
  mgr->addDagBlock(blkF);
  mgr->addDagBlock(blkE);
  mgr->addDagBlock(blkD);
  taraxa::thisThreadSleepForMilliSeconds(100);
  mgr->addDagBlock(blkH);
  mgr->addDagBlock(blkI);
  mgr->addDagBlock(blkG);
  mgr->addDagBlock(blkJ);
  mgr->addDagBlock(blkK);
  taraxa::thisThreadSleepForMilliSeconds(100);

  vec_blk_t orders;
  uint64_t period;
  period = mgr->getDagBlockOrder(blkA.getHash(), orders);
  EXPECT_EQ(orders.size(), 1);
  EXPECT_EQ(period, 1);
  // repeat, should not change
  period = mgr->getDagBlockOrder(blkA.getHash(), orders);
  EXPECT_EQ(orders.size(), 1);
  EXPECT_EQ(period, 1);

  mgr->setDagBlockPeriod(blkA.getHash(), period);

  period = mgr->getDagBlockOrder(blkC.getHash(), orders);
  EXPECT_EQ(orders.size(), 2);
  EXPECT_EQ(period, 2);
  // repeat, should not change
  period = mgr->getDagBlockOrder(blkC.getHash(), orders);
  EXPECT_EQ(orders.size(), 2);
  EXPECT_EQ(period, 2);

  mgr->setDagBlockPeriod(blkC.getHash(), period);

  period = mgr->getDagBlockOrder(blkE.getHash(), orders);
  EXPECT_EQ(orders.size(), 3);
  EXPECT_EQ(period, 3);
  mgr->setDagBlockPeriod(blkE.getHash(), period);

  period = mgr->getDagBlockOrder(blkH.getHash(), orders);
  EXPECT_EQ(orders.size(), 4);
  EXPECT_EQ(period, 4);
  mgr->setDagBlockPeriod(blkH.getHash(), period);

  if (orders.size() == 4) {
    EXPECT_EQ(orders[0], blk_hash_t(10));
    EXPECT_EQ(orders[1], blk_hash_t(9));
    EXPECT_EQ(orders[2], blk_hash_t(7));
    EXPECT_EQ(orders[3], blk_hash_t(8));
  }
  period = mgr->getDagBlockOrder(blkK.getHash(), orders);
  EXPECT_EQ(orders.size(), 1);
  EXPECT_EQ(period, 5);
  mgr->setDagBlockPeriod(blkK.getHash(), period);

  mgr->deletePeriod(0);  // should be no op
  mgr->deletePeriod(1);
  EXPECT_EQ(mgr->getNumVerticesInDag(), std::make_pair(11ull, 11ull));
  mgr->deletePeriod(2);
  EXPECT_EQ(mgr->getNumVerticesInDag(), std::make_pair(9ull, 9ull));
  mgr->deletePeriod(4);
  EXPECT_EQ(mgr->getNumVerticesInDag(), std::make_pair(5ull, 5ull));
  mgr->deletePeriod(3);
  EXPECT_EQ(mgr->getNumVerticesInDag(), std::make_pair(2ull, 2ull));
  mgr->deletePeriod(5);  
  EXPECT_EQ(mgr->getNumVerticesInDag(), std::make_pair(1ull, 1ull));
}

TEST(DagManager, receive_block_out_of_order) {
  const std::string GENESIS =
      "0000000000000000000000000000000000000000000000000000000000000000";
  auto mgr = std::make_shared<DagManager>(GENESIS);
  mgr->start();

  // mgr.setVerbose(true);
  DagBlock blk1(blk_hash_t(0), 0, {}, {}, sig_t(777), blk_hash_t(1),
                addr_t(15));
  DagBlock blk2(blk_hash_t(1), 0, {}, {}, sig_t(777), blk_hash_t(2),
                addr_t(15));
  DagBlock blk3(blk_hash_t(0), 0, {blk_hash_t(1), blk_hash_t(2)}, {},
                sig_t(777), blk_hash_t(3), addr_t(15));

  mgr->addDagBlock(blk3);
  mgr->addDagBlock(blk2);
  mgr->addDagBlock(blk1);
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
  const std::string GENESIS =
      "0000000000000000000000000000000000000000000000000000000000000000";
  auto mgr = std::make_shared<DagManager>(GENESIS);
  mgr->start();

  // mgr.setVerbose(true);
  DagBlock blk1(blk_hash_t(0), 0, {}, {}, sig_t(0), blk_hash_t(1), addr_t(15));
  DagBlock blk2(blk_hash_t(1), 0, {}, {}, sig_t(1), blk_hash_t(2), addr_t(15));
  DagBlock blk3(blk_hash_t(2), 0, {}, {}, sig_t(1), blk_hash_t(3), addr_t(15));
  DagBlock blk4(blk_hash_t(1), 0, {}, {}, sig_t(1), blk_hash_t(4), addr_t(15));
  DagBlock blk5(blk_hash_t(4), 0, {}, {}, sig_t(1), blk_hash_t(5), addr_t(15));
  DagBlock blk6(blk_hash_t(2), 0, {blk_hash_t(5)}, {}, sig_t(1), blk_hash_t(6),
                addr_t(15));
  mgr->addDagBlock(blk3);
  mgr->addDagBlock(blk6);
  mgr->addDagBlock(blk4);
  mgr->addDagBlock(blk5);
  mgr->addDagBlock(blk2);
  mgr->addDagBlock(blk1);
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

}  // namespace taraxa

int main(int argc, char** argv) {
  TaraxaStackTrace st;
  dev::LoggingOptions logOptions;
  logOptions.verbosity = dev::VerbosityWarning;
  dev::setupLogging(logOptions);
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}