#include "dag/dag.hpp"

#include <gtest/gtest.h>

#include "common/static_init.hpp"
#include "common/types.hpp"
#include "logger/logger.hpp"
#include "util_test/util.hpp"

namespace taraxa::core_tests {

struct DagTest : BaseTest {
  std::vector<FullNodeConfig> node_cfgs = make_node_cfgs(1);
  logger::Logger time_log = logger::createLogger(logger::Verbosity::Info, "TMSTM", addr_t());
};

TEST_F(DagTest, build_dag) {
  const blk_hash_t GENESIS("0000000000000000000000000000000000000000000000000000000000000001");
  taraxa::Dag graph(GENESIS, addr_t());

  // a genesis vertex
  EXPECT_EQ(1, graph.getNumVertices());

  blk_hash_t v1("0000000000000000000000000000000000000000000000000000000000000002");
  blk_hash_t v2("0000000000000000000000000000000000000000000000000000000000000003");
  blk_hash_t v3("0000000000000000000000000000000000000000000000000000000000000004");

  std::vector<blk_hash_t> empty;
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
  const blk_hash_t GENESIS("0000000000000000000000000000000000000000000000000000000000000001");
  taraxa::Dag graph(GENESIS, addr_t());

  // a genesis vertex
  EXPECT_EQ(1, graph.getNumVertices());

  blk_hash_t v1("0000000000000000000000000000000000000000000000000000000000000002");
  blk_hash_t v2("0000000000000000000000000000000000000000000000000000000000000003");
  blk_hash_t v3("0000000000000000000000000000000000000000000000000000000000000004");
  blk_hash_t v4("0000000000000000000000000000000000000000000000000000000000000005");
  blk_hash_t v5("0000000000000000000000000000000000000000000000000000000000000006");
  blk_hash_t v6("0000000000000000000000000000000000000000000000000000000000000007");
  blk_hash_t v7("0000000000000000000000000000000000000000000000000000000000000008");
  blk_hash_t v8("0000000000000000000000000000000000000000000000000000000000000009");
  blk_hash_t v9("000000000000000000000000000000000000000000000000000000000000000a");

  std::vector<blk_hash_t> empty;
  blk_hash_t no;
  // isolate node
  graph.addVEEs(v1, no, empty);
  graph.addVEEs(v2, no, empty);
  EXPECT_EQ(3, graph.getNumVertices());
  EXPECT_EQ(0, graph.getNumEdges());

  std::vector<blk_hash_t> leaves;
  blk_hash_t pivot;
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
  const blk_hash_t GENESIS("0000000000000000000000000000000000000000000000000000000000000001");
  taraxa::Dag graph(GENESIS, addr_t());

  // a genesis vertex
  EXPECT_EQ(1, graph.getNumVertices());

  blk_hash_t v1("0000000000000000000000000000000000000000000000000000000000000002");
  blk_hash_t v2("0000000000000000000000000000000000000000000000000000000000000003");
  blk_hash_t v3("0000000000000000000000000000000000000000000000000000000000000004");
  blk_hash_t v4("0000000000000000000000000000000000000000000000000000000000000005");
  blk_hash_t v5("0000000000000000000000000000000000000000000000000000000000000006");
  blk_hash_t v6("0000000000000000000000000000000000000000000000000000000000000007");

  std::vector<blk_hash_t> empty;
  blk_hash_t no;

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
  const blk_hash_t GENESIS("0000000000000000000000000000000000000000000000000000000000000001");
  taraxa::PivotTree graph(GENESIS, addr_t());

  std::vector<blk_hash_t> pivot_chain, leaves;
  graph.getGhostPath(GENESIS, pivot_chain);
  EXPECT_EQ(pivot_chain.size(), 1);
  graph.getLeaves(leaves);
  EXPECT_EQ(leaves.size(), 1);
}

// Use the example on Conflux paper
TEST_F(DagTest, compute_epoch) {
  const blk_hash_t GENESIS("0000000000000000000000000000000000000000000000000000000000000001");
  auto db_ptr = std::make_shared<DbStorage>(data_dir / "db");
  auto mgr = std::make_shared<DagManager>(
      GENESIS, addr_t(), nullptr, nullptr,
      std::make_shared<DagBlockManager>(addr_t(), node_cfgs[0].chain.vdf, node_cfgs[0].chain.final_chain.state.dpos, 1,
                                        db_ptr, nullptr, nullptr, nullptr, time_log),
      db_ptr, logger::Logger());
  DagBlock blkA(blk_hash_t(1), 0, {}, {trx_hash_t(2)}, sig_t(1), blk_hash_t(2), addr_t(1));
  DagBlock blkB(blk_hash_t(1), 0, {}, {trx_hash_t(3), trx_hash_t(4)}, sig_t(1), blk_hash_t(3), addr_t(1));
  DagBlock blkC(blk_hash_t(2), 0, {blk_hash_t(3)}, {}, sig_t(1), blk_hash_t(4), addr_t(1));
  DagBlock blkD(blk_hash_t(2), 0, {}, {}, sig_t(1), blk_hash_t(5), addr_t(1));
  DagBlock blkE(blk_hash_t(4), 0, {blk_hash_t(5), blk_hash_t(7)}, {}, sig_t(1), blk_hash_t(6), addr_t(1));
  DagBlock blkF(blk_hash_t(3), 0, {}, {}, sig_t(1), blk_hash_t(7), addr_t(1));
  DagBlock blkG(blk_hash_t(2), 0, {}, {trx_hash_t(4)}, sig_t(1), blk_hash_t(8), addr_t(1));
  DagBlock blkH(blk_hash_t(6), 0, {blk_hash_t(8), blk_hash_t(10)}, {}, sig_t(1), blk_hash_t(9), addr_t(1));
  DagBlock blkI(blk_hash_t(11), 0, {blk_hash_t(4)}, {}, sig_t(1), blk_hash_t(10), addr_t(1));
  DagBlock blkJ(blk_hash_t(7), 0, {}, {}, sig_t(1), blk_hash_t(11), addr_t(1));
  DagBlock blkK(blk_hash_t(9), 0, {}, {}, sig_t(1), blk_hash_t(12), addr_t(1));
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

  vec_blk_t orders;
  uint64_t period;

  // Test that with incorrect period, order is not returned
  orders = mgr->getDagBlockOrder(blkA.getHash(), 2);
  EXPECT_EQ(orders.size(), 0);

  orders = mgr->getDagBlockOrder(blkA.getHash(), 0);
  EXPECT_EQ(orders.size(), 0);

  period = 1;
  orders = mgr->getDagBlockOrder(blkA.getHash(), period);
  EXPECT_EQ(orders.size(), 1);
  // repeat, should not change
  orders = mgr->getDagBlockOrder(blkA.getHash(), period);
  EXPECT_EQ(orders.size(), 1);

  mgr->setDagBlockOrder(blkA.getHash(), period, orders);

  period = 2;
  orders = mgr->getDagBlockOrder(blkC.getHash(), period);
  EXPECT_EQ(orders.size(), 2);
  // repeat, should not change
  orders = mgr->getDagBlockOrder(blkC.getHash(), period);
  EXPECT_EQ(orders.size(), 2);

  mgr->setDagBlockOrder(blkC.getHash(), period, orders);

  period = 3;
  orders = mgr->getDagBlockOrder(blkE.getHash(), period);
  EXPECT_EQ(orders.size(), period);
  mgr->setDagBlockOrder(blkE.getHash(), period, orders);

  period = 4;
  orders = mgr->getDagBlockOrder(blkH.getHash(), period);
  EXPECT_EQ(orders.size(), period);
  mgr->setDagBlockOrder(blkH.getHash(), period, orders);

  if (orders.size() == 4) {
    EXPECT_EQ(orders[0], blk_hash_t(11));
    EXPECT_EQ(orders[1], blk_hash_t(10));
    EXPECT_EQ(orders[2], blk_hash_t(8));
    EXPECT_EQ(orders[3], blk_hash_t(9));
  }

  period = 5;
  orders = mgr->getDagBlockOrder(blkK.getHash(), period);
  EXPECT_EQ(orders.size(), 1);
  mgr->setDagBlockOrder(blkK.getHash(), period, orders);
}

TEST_F(DagTest, receive_block_in_order) {
  const blk_hash_t GENESIS("000000000000000000000000000000000000000000000000000000000000000a");
  auto db_ptr = std::make_shared<DbStorage>(data_dir / "db");
  auto mgr = std::make_shared<DagManager>(
      GENESIS, addr_t(), nullptr, nullptr,
      std::make_shared<DagBlockManager>(addr_t(), node_cfgs[0].chain.vdf, node_cfgs[0].chain.final_chain.state.dpos, 1,
                                        db_ptr, nullptr, nullptr, nullptr, time_log),
      db_ptr, logger::Logger());
  // mgr.setVerbose(true);
  DagBlock genesis_block(blk_hash_t(0), 0, {}, {}, sig_t(777), blk_hash_t(10), addr_t(15));
  DagBlock blk1(blk_hash_t(10), 0, {}, {}, sig_t(777), blk_hash_t(1), addr_t(15));
  DagBlock blk2(blk_hash_t(1), 0, {}, {}, sig_t(777), blk_hash_t(2), addr_t(15));
  DagBlock blk3(blk_hash_t(10), 0, {blk_hash_t(1), blk_hash_t(2)}, {}, sig_t(777), blk_hash_t(3), addr_t(15));

  mgr->addDagBlock(genesis_block);
  mgr->addDagBlock(blk1);
  mgr->addDagBlock(blk2);
  EXPECT_EQ(mgr->getNumVerticesInDag().first, 3);
  EXPECT_EQ(mgr->getNumEdgesInDag().first, 2);

  mgr->addDagBlock(blk3);
  taraxa::thisThreadSleepForMilliSeconds(500);

  auto ret = mgr->getLatestPivotAndTips();

  EXPECT_EQ(ret->first, blk_hash_t("0000000000000000000000000000000000000000000000000000000000000002"));
  EXPECT_EQ(ret->second.size(), 1);
  EXPECT_EQ(mgr->getNumVerticesInDag().first, 4);
  // total edges
  EXPECT_EQ(mgr->getNumEdgesInDag().first, 5);
}

// Use the example on Conflux paper, insert block in different order and make
// sure block order are the same
TEST_F(DagTest, compute_epoch_2) {
  const blk_hash_t GENESIS("0000000000000000000000000000000000000000000000000000000000000001");
  auto db_ptr = std::make_shared<DbStorage>(data_dir / "db");
  auto mgr = std::make_shared<DagManager>(
      GENESIS, addr_t(), nullptr, nullptr,
      std::make_shared<DagBlockManager>(addr_t(), node_cfgs[0].chain.vdf, node_cfgs[0].chain.final_chain.state.dpos, 1,
                                        db_ptr, nullptr, nullptr, nullptr, time_log),
      db_ptr, logger::Logger());
  DagBlock blkA(blk_hash_t(1), 0, {}, {trx_hash_t(2)}, sig_t(1), blk_hash_t(2), addr_t(1));
  DagBlock blkB(blk_hash_t(1), 0, {}, {trx_hash_t(3), trx_hash_t(4)}, sig_t(1), blk_hash_t(3), addr_t(1));
  DagBlock blkC(blk_hash_t(2), 0, {blk_hash_t(3)}, {}, sig_t(1), blk_hash_t(4), addr_t(1));
  DagBlock blkD(blk_hash_t(2), 0, {}, {}, sig_t(1), blk_hash_t(5), addr_t(1));
  DagBlock blkE(blk_hash_t(4), 0, {blk_hash_t(5), blk_hash_t(7)}, {}, sig_t(1), blk_hash_t(6), addr_t(1));
  DagBlock blkF(blk_hash_t(3), 0, {}, {}, sig_t(1), blk_hash_t(7), addr_t(1));
  DagBlock blkG(blk_hash_t(2), 0, {}, {trx_hash_t(4)}, sig_t(1), blk_hash_t(8), addr_t(1));
  DagBlock blkH(blk_hash_t(6), 0, {blk_hash_t(8), blk_hash_t(10)}, {}, sig_t(1), blk_hash_t(9), addr_t(1));
  DagBlock blkI(blk_hash_t(11), 0, {blk_hash_t(4)}, {}, sig_t(1), blk_hash_t(10), addr_t(1));
  DagBlock blkJ(blk_hash_t(7), 0, {}, {}, sig_t(1), blk_hash_t(11), addr_t(1));
  DagBlock blkK(blk_hash_t(10), 0, {}, {}, sig_t(1), blk_hash_t(12), addr_t(1));

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

  vec_blk_t orders;
  uint64_t period = 1;
  orders = mgr->getDagBlockOrder(blkA.getHash(), period);
  EXPECT_EQ(orders.size(), 1);
  // repeat, should not change
  orders = mgr->getDagBlockOrder(blkA.getHash(), period);
  EXPECT_EQ(orders.size(), 1);

  mgr->setDagBlockOrder(blkA.getHash(), period, orders);

  period = 2;
  orders = mgr->getDagBlockOrder(blkC.getHash(), period);
  EXPECT_EQ(orders.size(), 2);
  // repeat, should not change
  orders = mgr->getDagBlockOrder(blkC.getHash(), period);
  EXPECT_EQ(orders.size(), 2);

  mgr->setDagBlockOrder(blkC.getHash(), period, orders);

  period = 3;
  orders = mgr->getDagBlockOrder(blkE.getHash(), period);
  EXPECT_EQ(orders.size(), period);
  mgr->setDagBlockOrder(blkE.getHash(), period, orders);

  period = 4;
  orders = mgr->getDagBlockOrder(blkH.getHash(), period);
  EXPECT_EQ(orders.size(), 4);
  mgr->setDagBlockOrder(blkH.getHash(), period, orders);

  if (orders.size() == 4) {
    EXPECT_EQ(orders[0], blk_hash_t(11));
    EXPECT_EQ(orders[1], blk_hash_t(10));
    EXPECT_EQ(orders[2], blk_hash_t(8));
    EXPECT_EQ(orders[3], blk_hash_t(9));
  }

  period = 5;
  orders = mgr->getDagBlockOrder(blkK.getHash(), period);
  EXPECT_EQ(orders.size(), 1);
  mgr->setDagBlockOrder(blkK.getHash(), period, orders);
}

TEST_F(DagTest, get_latest_pivot_tips) {
  const blk_hash_t GENESIS("0000000000000000000000000000000000000000000000000000000000000001");
  auto db_ptr = std::make_shared<DbStorage>(data_dir / "db");
  auto mgr = std::make_shared<DagManager>(
      GENESIS, addr_t(), nullptr, nullptr,
      std::make_shared<DagBlockManager>(addr_t(), node_cfgs[0].chain.vdf, node_cfgs[0].chain.final_chain.state.dpos, 1,
                                        db_ptr, nullptr, nullptr, nullptr, time_log),
      db_ptr, logger::Logger());

  // mgr.setVerbose(true);
  DagBlock blk1(blk_hash_t(0), 0, {}, {}, sig_t(0), blk_hash_t(1), addr_t(15));
  DagBlock blk2(blk_hash_t(1), 0, {}, {}, sig_t(1), blk_hash_t(2), addr_t(15));
  DagBlock blk3(blk_hash_t(2), 0, {}, {}, sig_t(1), blk_hash_t(3), addr_t(15));
  DagBlock blk4(blk_hash_t(1), 0, {}, {}, sig_t(1), blk_hash_t(4), addr_t(15));
  DagBlock blk5(blk_hash_t(4), 0, {}, {}, sig_t(1), blk_hash_t(5), addr_t(15));
  DagBlock blk6(blk_hash_t(2), 0, {blk_hash_t(5)}, {}, sig_t(1), blk_hash_t(6), addr_t(15));
  mgr->addDagBlock(blk1);
  mgr->addDagBlock(blk2);
  mgr->addDagBlock(blk3);
  mgr->addDagBlock(blk4);
  mgr->addDagBlock(blk5);
  mgr->addDagBlock(blk6);
  taraxa::thisThreadSleepForMilliSeconds(100);

  auto ret = mgr->getLatestPivotAndTips();

  EXPECT_EQ(ret->first, blk_hash_t("0000000000000000000000000000000000000000000000000000000000000003"));
  EXPECT_EQ(ret->second.size(), 1);
  EXPECT_EQ(ret->second[0], blk_hash_t("0000000000000000000000000000000000000000000000000000000000000006"));
}

}  // namespace taraxa::core_tests

using namespace taraxa;
int main(int argc, char** argv) {
  static_init();
  auto logging = logger::createDefaultLoggingConfig();
  logging.verbosity = logger::Verbosity::Error;

  addr_t node_addr;
  logger::InitLogging(logging, node_addr);

  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}