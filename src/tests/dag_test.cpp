#include "../dag.hpp"

#include <gtest/gtest.h>

#include "../static_init.hpp"
#include "../types.hpp"
#include "../util_test/util.hpp"

namespace taraxa::core_tests {

struct DagTest : BaseTest {};

TEST_F(DagTest, build_dag) {
  const std::string GENESIS =
      "0000000000000000000000000000000000000000000000000000000000000000";
  taraxa::Dag graph(GENESIS, addr_t());

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

TEST_F(DagTest, dag_traverse_get_children_tips) {
  const std::string GENESIS =
      "0000000000000000000000000000000000000000000000000000000000000000";
  taraxa::Dag graph(GENESIS, addr_t());

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

TEST_F(DagTest, dag_traverse2_get_children_tips) {
  const std::string GENESIS =
      "0000000000000000000000000000000000000000000000000000000000000000";
  taraxa::Dag graph(GENESIS, addr_t());

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

TEST_F(DagTest, genesis_get_pivot) {
  const std::string GENESIS =
      "0000000000000000000000000000000000000000000000000000000000000000";
  taraxa::PivotTree graph(GENESIS, addr_t());

  std::vector<std::string> pivot_chain, leaves;
  graph.getGhostPath(GENESIS, pivot_chain);
  EXPECT_EQ(pivot_chain.size(), 1);
  graph.getLeaves(leaves);
  EXPECT_EQ(leaves.size(), 1);
}

// Use the example on Conflux paper
TEST_F(DagTest, compute_epoch) {
  const std::string GENESIS =
      "0000000000000000000000000000000000000000000000000000000000000000";
  auto db_ptr = s_ptr(new DbStorage(data_dir / "db"));
  auto mgr =
      std::make_shared<DagManager>(GENESIS, addr_t(), nullptr, nullptr, db_ptr);
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
  mgr->addDagBlock(blkF);
  taraxa::thisThreadSleepForMilliSeconds(100);
  mgr->addDagBlock(blkE);
  mgr->addDagBlock(blkG);
  mgr->addDagBlock(blkJ);
  mgr->addDagBlock(blkI);
  mgr->addDagBlock(blkH);
  mgr->addDagBlock(blkK);
  taraxa::thisThreadSleepForMilliSeconds(100);

  auto orders = std::make_shared<vec_blk_t>();
  uint64_t period;
  std::tie(period, orders) = mgr->getDagBlockOrder(blkA.getHash());
  EXPECT_EQ(orders->size(), 1);
  EXPECT_EQ(period, 1);
  // repeat, should not change
  std::tie(period, orders) = mgr->getDagBlockOrder(blkA.getHash());
  EXPECT_EQ(orders->size(), 1);
  EXPECT_EQ(period, 1);

  auto write_batch = db_ptr->createWriteBatch();
  mgr->setDagBlockOrder(blkA.getHash(), period, *orders, write_batch);
  db_ptr->commitWriteBatch(write_batch);

  std::tie(period, orders) = mgr->getDagBlockOrder(blkC.getHash());
  EXPECT_EQ(orders->size(), 2);
  EXPECT_EQ(period, 2);
  // repeat, should not change
  std::tie(period, orders) = mgr->getDagBlockOrder(blkC.getHash());
  EXPECT_EQ(orders->size(), 2);
  EXPECT_EQ(period, 2);

  write_batch = db_ptr->createWriteBatch();
  mgr->setDagBlockOrder(blkC.getHash(), period, *orders, write_batch);
  db_ptr->commitWriteBatch(write_batch);

  std::tie(period, orders) = mgr->getDagBlockOrder(blkE.getHash());
  EXPECT_EQ(orders->size(), 3);
  EXPECT_EQ(period, 3);
  write_batch = db_ptr->createWriteBatch();
  mgr->setDagBlockOrder(blkE.getHash(), period, *orders, write_batch);
  db_ptr->commitWriteBatch(write_batch);

  std::tie(period, orders) = mgr->getDagBlockOrder(blkH.getHash());
  EXPECT_EQ(orders->size(), 4);
  EXPECT_EQ(period, 4);
  write_batch = db_ptr->createWriteBatch();
  mgr->setDagBlockOrder(blkH.getHash(), period, *orders, write_batch);
  db_ptr->commitWriteBatch(write_batch);

  if (orders->size() == 4) {
    EXPECT_EQ((*orders)[0], blk_hash_t(10));
    EXPECT_EQ((*orders)[1], blk_hash_t(9));
    EXPECT_EQ((*orders)[2], blk_hash_t(7));
    EXPECT_EQ((*orders)[3], blk_hash_t(8));
  }
  std::tie(period, orders) = mgr->getDagBlockOrder(blkK.getHash());
  EXPECT_EQ(orders->size(), 1);
  EXPECT_EQ(period, 5);
  write_batch = db_ptr->createWriteBatch();
  mgr->setDagBlockOrder(blkK.getHash(), period, *orders, write_batch);
  db_ptr->commitWriteBatch(write_batch);
}

TEST_F(DagTest, receive_block_in_order) {
  const std::string GENESIS =
      "000000000000000000000000000000000000000000000000000000000000000a";
  auto db_ptr = s_ptr(new DbStorage(data_dir / "db"));
  auto mgr =
      std::make_shared<DagManager>(GENESIS, addr_t(), nullptr, nullptr, db_ptr);
  // mgr.setVerbose(true);
  DagBlock genesis_block(blk_hash_t(0), 0, {}, {}, sig_t(777), blk_hash_t(10),
                         addr_t(15));
  DagBlock blk1(blk_hash_t(10), 0, {}, {}, sig_t(777), blk_hash_t(1),
                addr_t(15));
  DagBlock blk2(blk_hash_t(1), 0, {}, {}, sig_t(777), blk_hash_t(2),
                addr_t(15));
  DagBlock blk3(blk_hash_t(10), 0, {blk_hash_t(1), blk_hash_t(2)}, {},
                sig_t(777), blk_hash_t(3), addr_t(15));

  mgr->addDagBlock(genesis_block);
  mgr->addDagBlock(blk1);
  mgr->addDagBlock(blk2);
  EXPECT_EQ(mgr->getNumVerticesInDag().first, 3);
  EXPECT_EQ(mgr->getNumEdgesInDag().first, 2);

  mgr->addDagBlock(blk3);
  taraxa::thisThreadSleepForMilliSeconds(500);

  std::string pivot;
  std::vector<std::string> tips;
  std::vector<Dag::vertex_t> criticals;
  mgr->getLatestPivotAndTips(pivot, tips);

  EXPECT_EQ(pivot,
            "0000000000000000000000000000000000000000000000000000000000000002");
  EXPECT_EQ(tips.size(), 1);
  EXPECT_EQ(mgr->getNumVerticesInDag().first, 4);
  // total edges
  EXPECT_EQ(mgr->getNumEdgesInDag().first, 5);
}

// Use the example on Conflux paper, insert block in different order and make
// sure block order are the same
TEST_F(DagTest, compute_epoch_2) {
  const std::string GENESIS =
      "0000000000000000000000000000000000000000000000000000000000000000";
  auto db_ptr = s_ptr(new DbStorage(data_dir / "db"));
  auto mgr =
      std::make_shared<DagManager>(GENESIS, addr_t(), nullptr, nullptr, db_ptr);
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
  mgr->addDagBlock(blkB);
  mgr->addDagBlock(blkC);
  mgr->addDagBlock(blkD);
  mgr->addDagBlock(blkF);
  mgr->addDagBlock(blkJ);
  mgr->addDagBlock(blkE);
  taraxa::thisThreadSleepForMilliSeconds(100);
  mgr->addDagBlock(blkG);
  mgr->addDagBlock(blkI);
  mgr->addDagBlock(blkH);
  mgr->addDagBlock(blkK);
  taraxa::thisThreadSleepForMilliSeconds(100);

  auto orders = std::make_shared<vec_blk_t>();
  uint64_t period;
  std::tie(period, orders) = mgr->getDagBlockOrder(blkA.getHash());
  EXPECT_EQ(orders->size(), 1);
  EXPECT_EQ(period, 1);
  // repeat, should not change
  std::tie(period, orders) = mgr->getDagBlockOrder(blkA.getHash());
  EXPECT_EQ(orders->size(), 1);
  EXPECT_EQ(period, 1);

  auto write_batch = db_ptr->createWriteBatch();
  mgr->setDagBlockOrder(blkA.getHash(), period, *orders, write_batch);
  db_ptr->commitWriteBatch(write_batch);

  std::tie(period, orders) = mgr->getDagBlockOrder(blkC.getHash());
  EXPECT_EQ(orders->size(), 2);
  EXPECT_EQ(period, 2);
  // repeat, should not change
  std::tie(period, orders) = mgr->getDagBlockOrder(blkC.getHash());
  EXPECT_EQ(orders->size(), 2);
  EXPECT_EQ(period, 2);

  write_batch = db_ptr->createWriteBatch();
  mgr->setDagBlockOrder(blkC.getHash(), period, *orders, write_batch);
  db_ptr->commitWriteBatch(write_batch);

  std::tie(period, orders) = mgr->getDagBlockOrder(blkE.getHash());
  EXPECT_EQ(orders->size(), 3);
  EXPECT_EQ(period, 3);
  write_batch = db_ptr->createWriteBatch();
  mgr->setDagBlockOrder(blkE.getHash(), period, *orders, write_batch);
  db_ptr->commitWriteBatch(write_batch);

  std::tie(period, orders) = mgr->getDagBlockOrder(blkH.getHash());
  EXPECT_EQ(orders->size(), 4);
  EXPECT_EQ(period, 4);
  write_batch = db_ptr->createWriteBatch();
  mgr->setDagBlockOrder(blkH.getHash(), period, *orders, write_batch);
  db_ptr->commitWriteBatch(write_batch);

  if (orders->size() == 4) {
    EXPECT_EQ((*orders)[0], blk_hash_t(10));
    EXPECT_EQ((*orders)[1], blk_hash_t(9));
    EXPECT_EQ((*orders)[2], blk_hash_t(7));
    EXPECT_EQ((*orders)[3], blk_hash_t(8));
  }
  std::tie(period, orders) = mgr->getDagBlockOrder(blkK.getHash());
  EXPECT_EQ(orders->size(), 1);
  EXPECT_EQ(period, 5);
  write_batch = db_ptr->createWriteBatch();
  mgr->setDagBlockOrder(blkK.getHash(), period, *orders, write_batch);
  db_ptr->commitWriteBatch(write_batch);
}

TEST_F(DagTest, get_latest_pivot_tips) {
  const std::string GENESIS =
      "0000000000000000000000000000000000000000000000000000000000000000";
  auto db_ptr = s_ptr(new DbStorage(data_dir / "db"));
  auto mgr =
      std::make_shared<DagManager>(GENESIS, addr_t(), nullptr, nullptr, db_ptr);

  // mgr.setVerbose(true);
  DagBlock blk1(blk_hash_t(0), 0, {}, {}, sig_t(0), blk_hash_t(1), addr_t(15));
  DagBlock blk2(blk_hash_t(1), 0, {}, {}, sig_t(1), blk_hash_t(2), addr_t(15));
  DagBlock blk3(blk_hash_t(2), 0, {}, {}, sig_t(1), blk_hash_t(3), addr_t(15));
  DagBlock blk4(blk_hash_t(1), 0, {}, {}, sig_t(1), blk_hash_t(4), addr_t(15));
  DagBlock blk5(blk_hash_t(4), 0, {}, {}, sig_t(1), blk_hash_t(5), addr_t(15));
  DagBlock blk6(blk_hash_t(2), 0, {blk_hash_t(5)}, {}, sig_t(1), blk_hash_t(6),
                addr_t(15));
  mgr->addDagBlock(blk1);
  mgr->addDagBlock(blk2);
  mgr->addDagBlock(blk3);
  mgr->addDagBlock(blk4);
  mgr->addDagBlock(blk5);
  mgr->addDagBlock(blk6);
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
}

}  // namespace taraxa::core_tests

using namespace taraxa;
int main(int argc, char** argv) {
  static_init();
  LoggingConfig logging;
  addr_t node_addr;
  logging.verbosity = taraxa::VerbosityError;
  setupLoggingConfiguration(node_addr, logging);

  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}