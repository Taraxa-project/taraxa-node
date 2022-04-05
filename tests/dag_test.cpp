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
  auto trx_mgr = std::make_shared<TransactionManager>(FullNodeConfig(), db_ptr, nullptr, addr_t());
  auto pbft_chain = std::make_shared<PbftChain>(GENESIS, addr_t(), db_ptr);
  auto mgr =
      std::make_shared<DagManager>(GENESIS, addr_t(), trx_mgr, pbft_chain,
                                   std::make_shared<DagBlockManager>(addr_t(), node_cfgs[0].chain.sortition, db_ptr,
                                                                     nullptr, nullptr, pbft_chain, time_log),
                                   db_ptr, logger::Logger());
  DagBlock blkA(blk_hash_t(1), 0, {}, {trx_hash_t(2)}, sig_t(1), blk_hash_t(2), addr_t(1));
  DagBlock blkB(blk_hash_t(1), 0, {}, {trx_hash_t(3), trx_hash_t(4)}, sig_t(1), blk_hash_t(3), addr_t(1));
  DagBlock blkC(blk_hash_t(2), 1, {blk_hash_t(3)}, {}, sig_t(1), blk_hash_t(4), addr_t(1));
  DagBlock blkD(blk_hash_t(2), 1, {}, {}, sig_t(1), blk_hash_t(5), addr_t(1));
  DagBlock blkE(blk_hash_t(4), 2, {blk_hash_t(5), blk_hash_t(7)}, {}, sig_t(1), blk_hash_t(6), addr_t(1));
  DagBlock blkF(blk_hash_t(3), 1, {}, {}, sig_t(1), blk_hash_t(7), addr_t(1));
  DagBlock blkG(blk_hash_t(2), 1, {}, {trx_hash_t(4)}, sig_t(1), blk_hash_t(8), addr_t(1));
  DagBlock blkH(blk_hash_t(6), 4, {blk_hash_t(8), blk_hash_t(10)}, {}, sig_t(1), blk_hash_t(9), addr_t(1));
  DagBlock blkI(blk_hash_t(11), 3, {blk_hash_t(4)}, {}, sig_t(1), blk_hash_t(10), addr_t(1));
  DagBlock blkJ(blk_hash_t(7), 2, {}, {}, sig_t(1), blk_hash_t(11), addr_t(1));
  DagBlock blkK(blk_hash_t(9), 5, {}, {}, sig_t(1), blk_hash_t(12), addr_t(1));

  const auto blkA_hash = blkA.getHash();
  const auto blkC_hash = blkC.getHash();
  const auto blkE_hash = blkE.getHash();
  const auto blkH_hash = blkH.getHash();
  const auto blkK_hash = blkK.getHash();

  mgr->addDagBlock(std::move(blkA));
  mgr->addDagBlock(std::move(blkB));
  mgr->addDagBlock(std::move(blkC));
  mgr->addDagBlock(std::move(blkD));
  mgr->addDagBlock(std::move(blkF));
  taraxa::thisThreadSleepForMilliSeconds(100);
  mgr->addDagBlock(std::move(blkE));
  mgr->addDagBlock(std::move(blkG));
  mgr->addDagBlock(std::move(blkJ));
  mgr->addDagBlock(std::move(blkI));
  mgr->addDagBlock(std::move(blkH));
  mgr->addDagBlock(std::move(blkK));
  taraxa::thisThreadSleepForMilliSeconds(100);

  vec_blk_t orders;
  uint64_t period;

  // Test that with incorrect period, order is not returned
  orders = mgr->getDagBlockOrder(blkA_hash, 2);
  EXPECT_EQ(orders.size(), 0);

  orders = mgr->getDagBlockOrder(blkA_hash, 0);
  EXPECT_EQ(orders.size(), 0);

  period = 1;
  orders = mgr->getDagBlockOrder(blkA_hash, period);
  EXPECT_EQ(orders.size(), 1);
  // repeat, should not change
  orders = mgr->getDagBlockOrder(blkA_hash, period);
  EXPECT_EQ(orders.size(), 1);

  mgr->setDagBlockOrder(blkA_hash, period, orders);

  period = 2;
  orders = mgr->getDagBlockOrder(blkC_hash, period);
  EXPECT_EQ(orders.size(), 2);
  // repeat, should not change
  orders = mgr->getDagBlockOrder(blkC_hash, period);
  EXPECT_EQ(orders.size(), 2);

  mgr->setDagBlockOrder(blkC_hash, period, orders);

  period = 3;
  orders = mgr->getDagBlockOrder(blkE_hash, period);
  EXPECT_EQ(orders.size(), period);
  mgr->setDagBlockOrder(blkE_hash, period, orders);

  period = 4;
  orders = mgr->getDagBlockOrder(blkH_hash, period);
  EXPECT_EQ(orders.size(), period);
  mgr->setDagBlockOrder(blkH_hash, period, orders);

  if (orders.size() == 4) {
    EXPECT_EQ(orders[0], blk_hash_t(11));
    EXPECT_EQ(orders[1], blk_hash_t(10));
    EXPECT_EQ(orders[2], blk_hash_t(8));
    EXPECT_EQ(orders[3], blk_hash_t(9));
  }

  period = 5;
  orders = mgr->getDagBlockOrder(blkK_hash, period);
  EXPECT_EQ(orders.size(), 1);
  mgr->setDagBlockOrder(blkK_hash, period, orders);
}

TEST_F(DagTest, dag_expiry) {
  const blk_hash_t GENESIS("0000000000000000000000000000000000000000000000000000000000000001");
  const uint32_t EXPIRY_LIMIT = 3;
  auto db_ptr = std::make_shared<DbStorage>(data_dir / "db");
  auto trx_mgr = std::make_shared<TransactionManager>(FullNodeConfig(), db_ptr, nullptr, addr_t());
  auto pbft_chain = std::make_shared<PbftChain>(GENESIS, addr_t(), db_ptr);
  auto dag_blk_mgr = std::make_shared<DagBlockManager>(addr_t(), node_cfgs[0].chain.sortition, db_ptr, nullptr, nullptr,
                                                       pbft_chain, time_log);
  auto mgr = std::make_shared<DagManager>(GENESIS, addr_t(), trx_mgr, pbft_chain, dag_blk_mgr, db_ptr, logger::Logger(),
                                          false, 0, 3, EXPIRY_LIMIT);
  DagBlock blkA(blk_hash_t(1), 0, {}, {trx_hash_t(2)}, sig_t(1), blk_hash_t(2), addr_t(1));
  DagBlock blkB(blk_hash_t(1), 0, {}, {trx_hash_t(3), trx_hash_t(4)}, sig_t(1), blk_hash_t(3), addr_t(1));
  DagBlock blkC(blk_hash_t(2), 1, {blk_hash_t(3)}, {}, sig_t(1), blk_hash_t(4), addr_t(1));
  DagBlock blkD(blk_hash_t(2), 1, {}, {}, sig_t(1), blk_hash_t(5), addr_t(1));
  DagBlock blkE(blk_hash_t(4), 2, {blk_hash_t(5), blk_hash_t(7)}, {}, sig_t(1), blk_hash_t(6), addr_t(1));
  DagBlock blkF(blk_hash_t(3), 1, {}, {}, sig_t(1), blk_hash_t(7), addr_t(1));
  DagBlock blkG(blk_hash_t(2), 1, {}, {trx_hash_t(4)}, sig_t(1), blk_hash_t(8), addr_t(1));
  DagBlock blkH(blk_hash_t(6), 4, {blk_hash_t(8), blk_hash_t(10)}, {}, sig_t(1), blk_hash_t(9), addr_t(1));
  DagBlock blkI(blk_hash_t(11), 3, {blk_hash_t(4)}, {}, sig_t(1), blk_hash_t(10), addr_t(1));
  DagBlock blkJ(blk_hash_t(7), 2, {}, {}, sig_t(1), blk_hash_t(11), addr_t(1));
  DagBlock blkK(blk_hash_t(9), 5, {}, {}, sig_t(1), blk_hash_t(12), addr_t(1));

  const auto blkK_hash = blkK.getHash();

  mgr->addDagBlock(std::move(blkA));
  mgr->addDagBlock(std::move(blkB));
  mgr->addDagBlock(std::move(blkC));
  mgr->addDagBlock(std::move(blkD));
  mgr->addDagBlock(std::move(blkF));
  mgr->addDagBlock(std::move(blkE));
  mgr->addDagBlock(std::move(blkG));
  mgr->addDagBlock(std::move(blkJ));
  mgr->addDagBlock(std::move(blkI));
  mgr->addDagBlock(std::move(blkH));
  mgr->addDagBlock(std::move(blkK));
  taraxa::thisThreadSleepForMilliSeconds(100);

  vec_blk_t orders;
  orders = mgr->getDagBlockOrder(blkK_hash, 1);

  // Verify all blocks made it to DAG
  EXPECT_EQ(orders.size(), 11);

  // Verify initial expiry level
  EXPECT_EQ(dag_blk_mgr->getDagExpiryLevel(), 0);
  mgr->setDagBlockOrder(blkK_hash, 1, orders);

  // Verify expiry level
  EXPECT_EQ(dag_blk_mgr->getDagExpiryLevel(), blkK.getLevel() - EXPIRY_LIMIT);

  DagBlock blk_under_limit(blk_hash_t(2), blkK.getLevel() - EXPIRY_LIMIT - 1, {}, {}, sig_t(1), blk_hash_t(13),
                           addr_t(1));
  DagBlock blk_at_limit(blk_hash_t(4), blkK.getLevel() - EXPIRY_LIMIT, {}, {}, sig_t(1), blk_hash_t(14), addr_t(1));
  DagBlock blk_over_limit(blk_hash_t(11), blkK.getLevel() - EXPIRY_LIMIT + 1, {}, {}, sig_t(1), blk_hash_t(15),
                          addr_t(1));

  // Block under limit is not accepted to DAG since it is expired
  EXPECT_FALSE(mgr->addDagBlock(std::move(blk_under_limit)));
  EXPECT_TRUE(mgr->addDagBlock(std::move(blk_at_limit)));
  EXPECT_TRUE(mgr->addDagBlock(std::move(blk_over_limit)));
  EXPECT_FALSE(db_ptr->dagBlockInDb(blk_under_limit.getHash()));
  EXPECT_TRUE(db_ptr->dagBlockInDb(blk_at_limit.getHash()));
  EXPECT_TRUE(db_ptr->dagBlockInDb(blk_over_limit.getHash()));

  DagBlock blk_new_anchor(blk_hash_t(12), 6, {}, {}, sig_t(1), blk_hash_t(16), addr_t(1));
  EXPECT_TRUE(mgr->addDagBlock(std::move(blk_new_anchor)));

  orders = mgr->getDagBlockOrder(blk_new_anchor.getHash(), 2);
  mgr->setDagBlockOrder(blk_new_anchor.getHash(), 2, orders);

  // Verify that the block blk_at_limit which was initially part of the DAG became expired once new anchor moved the
  // limit
  EXPECT_FALSE(db_ptr->dagBlockInDb(blk_under_limit.getHash()));
  EXPECT_FALSE(db_ptr->dagBlockInDb(blk_at_limit.getHash()));
  EXPECT_TRUE(db_ptr->dagBlockInDb(blk_over_limit.getHash()));
}

TEST_F(DagTest, receive_block_in_order) {
  const blk_hash_t GENESIS("000000000000000000000000000000000000000000000000000000000000000a");
  auto db_ptr = std::make_shared<DbStorage>(data_dir / "db");
  auto pbft_chain = std::make_shared<PbftChain>(GENESIS, addr_t(), db_ptr);
  auto trx_mgr = std::make_shared<TransactionManager>(FullNodeConfig(), db_ptr, nullptr, addr_t());
  auto mgr =
      std::make_shared<DagManager>(GENESIS, addr_t(), trx_mgr, pbft_chain,
                                   std::make_shared<DagBlockManager>(addr_t(), node_cfgs[0].chain.sortition, db_ptr,
                                                                     nullptr, nullptr, pbft_chain, time_log),
                                   db_ptr, logger::Logger());
  DagBlock genesis_block(blk_hash_t(0), 0, {}, {}, sig_t(777), blk_hash_t(10), addr_t(15));
  DagBlock blk1(blk_hash_t(10), 0, {}, {}, sig_t(777), blk_hash_t(1), addr_t(15));
  DagBlock blk2(blk_hash_t(1), 0, {}, {}, sig_t(777), blk_hash_t(2), addr_t(15));
  DagBlock blk3(blk_hash_t(10), 0, {blk_hash_t(1), blk_hash_t(2)}, {}, sig_t(777), blk_hash_t(3), addr_t(15));

  mgr->addDagBlock(std::move(genesis_block));
  mgr->addDagBlock(std::move(blk1));
  mgr->addDagBlock(std::move(blk2));
  EXPECT_EQ(mgr->getNumVerticesInDag().first, 3);
  EXPECT_EQ(mgr->getNumEdgesInDag().first, 2);

  mgr->addDagBlock(std::move(blk3));
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
  auto pbft_chain = std::make_shared<PbftChain>(GENESIS, addr_t(), db_ptr);
  auto trx_mgr = std::make_shared<TransactionManager>(FullNodeConfig(), db_ptr, nullptr, addr_t());
  auto mgr =
      std::make_shared<DagManager>(GENESIS, addr_t(), trx_mgr, pbft_chain,
                                   std::make_shared<DagBlockManager>(addr_t(), node_cfgs[0].chain.sortition, db_ptr,
                                                                     nullptr, nullptr, pbft_chain, time_log),
                                   db_ptr, logger::Logger());
  DagBlock blkA(blk_hash_t(1), 0, {}, {trx_hash_t(2)}, sig_t(1), blk_hash_t(2), addr_t(1));
  DagBlock blkB(blk_hash_t(1), 0, {}, {trx_hash_t(3), trx_hash_t(4)}, sig_t(1), blk_hash_t(3), addr_t(1));
  DagBlock blkC(blk_hash_t(2), 1, {blk_hash_t(3)}, {}, sig_t(1), blk_hash_t(4), addr_t(1));
  DagBlock blkD(blk_hash_t(2), 1, {}, {}, sig_t(1), blk_hash_t(5), addr_t(1));
  DagBlock blkE(blk_hash_t(4), 2, {blk_hash_t(5), blk_hash_t(7)}, {}, sig_t(1), blk_hash_t(6), addr_t(1));
  DagBlock blkF(blk_hash_t(3), 1, {}, {}, sig_t(1), blk_hash_t(7), addr_t(1));
  DagBlock blkG(blk_hash_t(2), 1, {}, {trx_hash_t(4)}, sig_t(1), blk_hash_t(8), addr_t(1));
  DagBlock blkH(blk_hash_t(6), 4, {blk_hash_t(8), blk_hash_t(10)}, {}, sig_t(1), blk_hash_t(9), addr_t(1));
  DagBlock blkI(blk_hash_t(11), 3, {blk_hash_t(4)}, {}, sig_t(1), blk_hash_t(10), addr_t(1));
  DagBlock blkJ(blk_hash_t(7), 2, {}, {}, sig_t(1), blk_hash_t(11), addr_t(1));
  DagBlock blkK(blk_hash_t(10), 4, {}, {}, sig_t(1), blk_hash_t(12), addr_t(1));

  const auto blkA_hash = blkA.getHash();
  const auto blkC_hash = blkC.getHash();
  const auto blkE_hash = blkE.getHash();
  const auto blkH_hash = blkH.getHash();
  const auto blkK_hash = blkK.getHash();

  mgr->addDagBlock(std::move(blkA));
  mgr->addDagBlock(std::move(blkB));
  mgr->addDagBlock(std::move(blkC));
  mgr->addDagBlock(std::move(blkD));
  mgr->addDagBlock(std::move(blkF));
  mgr->addDagBlock(std::move(blkJ));
  mgr->addDagBlock(std::move(blkE));
  taraxa::thisThreadSleepForMilliSeconds(100);
  mgr->addDagBlock(std::move(blkG));
  mgr->addDagBlock(std::move(blkI));
  mgr->addDagBlock(std::move(blkH));
  mgr->addDagBlock(std::move(blkK));
  taraxa::thisThreadSleepForMilliSeconds(100);

  vec_blk_t orders;
  uint64_t period = 1;
  orders = mgr->getDagBlockOrder(blkA_hash, period);
  EXPECT_EQ(orders.size(), 1);
  // repeat, should not change
  orders = mgr->getDagBlockOrder(blkA_hash, period);
  EXPECT_EQ(orders.size(), 1);

  mgr->setDagBlockOrder(blkA_hash, period, orders);

  period = 2;
  orders = mgr->getDagBlockOrder(blkC_hash, period);
  EXPECT_EQ(orders.size(), 2);
  // repeat, should not change
  orders = mgr->getDagBlockOrder(blkC_hash, period);
  EXPECT_EQ(orders.size(), 2);

  mgr->setDagBlockOrder(blkC_hash, period, orders);

  period = 3;
  orders = mgr->getDagBlockOrder(blkE_hash, period);
  EXPECT_EQ(orders.size(), period);
  mgr->setDagBlockOrder(blkE_hash, period, orders);

  period = 4;
  orders = mgr->getDagBlockOrder(blkH_hash, period);
  EXPECT_EQ(orders.size(), 4);
  mgr->setDagBlockOrder(blkH_hash, period, orders);

  if (orders.size() == 4) {
    EXPECT_EQ(orders[0], blk_hash_t(11));
    EXPECT_EQ(orders[1], blk_hash_t(10));
    EXPECT_EQ(orders[2], blk_hash_t(8));
    EXPECT_EQ(orders[3], blk_hash_t(9));
  }

  period = 5;
  orders = mgr->getDagBlockOrder(blkK_hash, period);
  EXPECT_EQ(orders.size(), 1);
  mgr->setDagBlockOrder(blkK_hash, period, orders);
}

TEST_F(DagTest, get_latest_pivot_tips) {
  const blk_hash_t GENESIS("0000000000000000000000000000000000000000000000000000000000000001");
  auto db_ptr = std::make_shared<DbStorage>(data_dir / "db");
  auto trx_mgr = std::make_shared<TransactionManager>(FullNodeConfig(), db_ptr, nullptr, addr_t());
  auto pbft_chain = std::make_shared<PbftChain>(GENESIS, addr_t(), db_ptr);
  auto mgr =
      std::make_shared<DagManager>(GENESIS, addr_t(), trx_mgr, pbft_chain,
                                   std::make_shared<DagBlockManager>(addr_t(), node_cfgs[0].chain.sortition, db_ptr,
                                                                     nullptr, nullptr, pbft_chain, time_log),
                                   db_ptr, logger::Logger());
  DagBlock blk1(blk_hash_t(0), 0, {}, {}, sig_t(0), blk_hash_t(1), addr_t(15));
  DagBlock blk2(blk_hash_t(1), 0, {}, {}, sig_t(1), blk_hash_t(2), addr_t(15));
  DagBlock blk3(blk_hash_t(2), 0, {}, {}, sig_t(1), blk_hash_t(3), addr_t(15));
  DagBlock blk4(blk_hash_t(1), 0, {}, {}, sig_t(1), blk_hash_t(4), addr_t(15));
  DagBlock blk5(blk_hash_t(4), 0, {}, {}, sig_t(1), blk_hash_t(5), addr_t(15));
  DagBlock blk6(blk_hash_t(2), 0, {blk_hash_t(5)}, {}, sig_t(1), blk_hash_t(6), addr_t(15));
  mgr->addDagBlock(std::move(blk1));
  mgr->addDagBlock(std::move(blk2));
  mgr->addDagBlock(std::move(blk3));
  mgr->addDagBlock(std::move(blk4));
  mgr->addDagBlock(std::move(blk5));
  mgr->addDagBlock(std::move(blk6));
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