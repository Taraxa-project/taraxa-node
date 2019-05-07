/*
 * @Copyright: Taraxa.io
 * @Author: Chia-Chun Lin
 * @Date: 2019-01-18 12:56:45
 * @Last Modified by: Chia-Chun Lin
 * @Last Modified time: 2019-04-23 18:24:45
 */
#include "full_node.hpp"
#include <gtest/gtest.h>
#include <atomic>
#include <boost/thread.hpp>
#include <iostream>
#include <vector>
#include "create_samples.hpp"
#include "dag.hpp"
#include "libdevcore/DBFactory.h"
#include "libdevcore/Log.h"
#include "network.hpp"
#include "pbft_chain.hpp"
#include "rpc.hpp"
#include "string"
#include "top.hpp"

namespace taraxa {

const unsigned NUM_TRX = 200;

auto g_secret = dev::Secret(
    "3800b2875669d9b2053c1aff9224ecfdc411423aac5b5a73d7a45ced1c3b9dcd",
    dev::Secret::ConstructFromStringType::FromHex);
auto g_key_pair = dev::KeyPair(g_secret);
auto g_trx_signed_samples =
    samples::createSignedTrxSamples(0, NUM_TRX, g_secret);
auto g_mock_dag0 = samples::createMockDag0();

TEST(FullNode, insert_anchor_and_compute_order) {
  boost::asio::io_context context;
  auto node(std::make_shared<taraxa::FullNode>(
      context, std::string("./core_tests/conf_taraxa1.json")));
  node->start(true /*boot_node*/);

  auto num_blks = g_mock_dag0.size();
  for (int i = 1; i <= 9; i++) {
    node->storeBlock(g_mock_dag0[i]);
  }
  taraxa::thisThreadSleepForMilliSeconds(200);
  std::string pivot;
  std::vector<std::string> tips;

  // -------- first period ----------

  node->getLatestPivotAndTips(pivot, tips);
  uint64_t period;
  std::shared_ptr<vec_blk_t> order;
  std::tie(period, order) =
      node->createPeriodAndComputeBlockOrder(blk_hash_t(pivot));
  EXPECT_EQ(period, 1);
  EXPECT_EQ(order->size(), 6);
  if (order->size() == 6) {
    (*order)[0] = blk_hash_t(2);
    (*order)[1] = blk_hash_t(3);
    (*order)[2] = blk_hash_t(6);
    (*order)[3] = blk_hash_t(1);
    (*order)[4] = blk_hash_t(5);
    (*order)[5] = blk_hash_t(7);
  }

  // -------- second period ----------

  for (int i = 10; i <= 16; i++) {
    node->storeBlock(g_mock_dag0[i]);
  }
  taraxa::thisThreadSleepForMilliSeconds(200);

  node->getLatestPivotAndTips(pivot, tips);
  std::tie(period, order) =
      node->createPeriodAndComputeBlockOrder(blk_hash_t(pivot));
  EXPECT_EQ(period, 2);
  if (order->size() == 7) {
    (*order)[0] = blk_hash_t(10);
    (*order)[1] = blk_hash_t(13);
    (*order)[2] = blk_hash_t(11);
    (*order)[3] = blk_hash_t(9);
    (*order)[4] = blk_hash_t(12);
    (*order)[5] = blk_hash_t(14);
    (*order)[5] = blk_hash_t(15);
  }

  // -------- third period ----------

  for (int i = 17; i < g_mock_dag0.size(); i++) {
    node->storeBlock(g_mock_dag0[i]);
  }
  taraxa::thisThreadSleepForMilliSeconds(200);

  node->getLatestPivotAndTips(pivot, tips);
  std::tie(period, order) =
      node->createPeriodAndComputeBlockOrder(blk_hash_t(pivot));
  EXPECT_EQ(period, 3);
  if (order->size() == 5) {
    (*order)[0] = blk_hash_t(17);
    (*order)[1] = blk_hash_t(16);
    (*order)[2] = blk_hash_t(8);
    (*order)[3] = blk_hash_t(18);
    (*order)[4] = blk_hash_t(19);
  }
}

TEST(Top, create_top) {
  {
    const char* inputs[] = {"./build/main", "--conf_taraxa",
                            "./core_tests/conf_taraxa1.json", "-v", "0"};
    Top top(5, inputs);
    taraxa::thisThreadSleepForSeconds(1);
    EXPECT_TRUE(top.isActive());
    top.stop();
    EXPECT_FALSE(top.isActive());
  }
}

TEST(Top, sync_two_nodes) {
  const char* input1[] = {"./build/main", "--conf_taraxa",
                          "./core_tests/conf_taraxa1.json", "-v", "0"};

  Top top1(5, input1);
  EXPECT_TRUE(top1.isActive());
  taraxa::thisThreadSleepForMilliSeconds(500);

  // send 1000 trxs
  try {
    std::cout << "Sending 1000 trxs ..." << std::endl;
    system("./core_tests/curl_send_1000_trx.sh");
  } catch (std::exception& e) {
    std::cerr << e.what() << std::endl;
  }

  // copy main2
  try {
    std::cout << "Copying main2 ..." << std::endl;
    system("cp ./build/main ./build/main2");
  } catch (std::exception& e) {
    std::cerr << e.what() << std::endl;
  }

  const char* input2[] = {"./build/main2", "--conf_taraxa",
                          "./core_tests/conf_taraxa2.json", "-v", "0"};
  Top top2(5, input2);
  EXPECT_TRUE(top2.isActive());
  std::cout << "Top2 created ..." << std::endl;
  // wait for top2 initialize
  taraxa::thisThreadSleepForMilliSeconds(1000);
  auto node1 = top1.getNode();
  auto node2 = top2.getNode();
  EXPECT_NE(node1, nullptr);
  EXPECT_NE(node2, nullptr);
  auto vertices1 = node1->getNumVerticesInDag();
  auto vertices2 = node2->getNumVerticesInDag();
  // add more delay if sync is not done
  for (auto i = 0; i < 60; i++) {
    if (vertices1 == vertices2) break;
    taraxa::thisThreadSleepForMilliSeconds(500);
    vertices1 = node1->getNumVerticesInDag();
    vertices2 = node2->getNumVerticesInDag();
    std::cout << "vertices1 = " << vertices1.first
              << " , vertices2 = " << vertices2.first << std::endl;
  }

  EXPECT_EQ(vertices1, vertices2);
  top2.stop();
  top1.stop();
  // delete main2
  try {
    std::cout << "main2 deleted ..." << std::endl;
    system("rm -f ./build/main2");
  } catch (std::exception& e) {
    std::cerr << e.what() << std::endl;
  }
}

TEST(FullNode, account_bal) {
  boost::asio::io_context context;

  auto node(std::make_shared<taraxa::FullNode>(
      context, std::string("./core_tests/conf_taraxa1.json")));
  addr_t addr1(100);
  bal_t bal1(1000);
  node->setBalance(addr1, bal1);
  auto res = node->getBalance(addr1);
  EXPECT_TRUE(res.second);
  EXPECT_EQ(res.first, bal1);
  addr_t addr2(200);
  res = node->getBalance(addr2);
  EXPECT_FALSE(res.second);
  bal_t bal2(2000);
  node->setBalance(addr1, bal2);
  res = node->getBalance(addr1);
  EXPECT_TRUE(res.second);
  EXPECT_EQ(res.first, bal2);
}

TEST(FullNode, execute_chain_pbft_transactions) {
  boost::asio::io_context context;

  auto node(std::make_shared<taraxa::FullNode>(
      context, std::string("./core_tests/conf_taraxa1.json")));
  node->start(true /*boot_node*/);
  addr_t acc1 = node->getAddress();
  bal_t bal1(10000000);
  node->setBalance(acc1, bal1);
  auto res = node->getBalance(acc1);
  EXPECT_TRUE(res.second);
  EXPECT_EQ(res.first, bal1);
  for (auto const& t : g_trx_signed_samples) {
    node->storeTransaction(t);
    taraxa::thisThreadSleepForMilliSeconds(50);
  }

  taraxa::thisThreadSleepForMilliSeconds(3000);

  EXPECT_GT(node->getNumProposedBlocks(), 0);

  // The test will form a single chain
  std::vector<std::string> ghost;
  node->getGhostPath(Dag::GENESIS, ghost);
  vec_blk_t blks;
  std::vector<std::vector<uint>> modes;

  uint64_t period = 0, cur_period;
  std::shared_ptr<vec_blk_t> order;
  // create a period for every 2 pivots
  for (int i = 1; i < ghost.size(); i += 2) {
    std::tie(cur_period, order) =
        node->createPeriodAndComputeBlockOrder(blk_hash_t(ghost[i]));
    EXPECT_EQ(cur_period, ++period);
    auto sche = node->createMockTrxSchedule(order);
    if (!sche) continue;
    ScheduleBlock sche_blk(blk_hash_t(100), 12345, sig_t(200), *sche);
    node->executeScheduleBlock(sche_blk);
    taraxa::thisThreadSleepForMilliSeconds(200);
  }
  // pickup the last period when dag (chain) size is odd number
  if (ghost.size() % 2) {
    std::tie(cur_period, order) =
        node->createPeriodAndComputeBlockOrder(blk_hash_t(ghost.back()));
    EXPECT_EQ(cur_period, ++period);
    auto sche = node->createMockTrxSchedule(order);
    ScheduleBlock sche_blk(blk_hash_t(100), 12345, sig_t(200), *sche);
    node->executeScheduleBlock(sche_blk);
    taraxa::thisThreadSleepForMilliSeconds(200);
  }

  node->stop();

  for (auto const& t : g_trx_signed_samples) {
    auto res = node->getBalance(t.getReceiver());
    EXPECT_TRUE(res.second);
    EXPECT_EQ(res.first, t.getValue());
  }
}

TEST(FullNode, send_and_receive_out_order_messages) {
  boost::asio::io_context context1;
  boost::asio::io_context context2;

  auto node1(std::make_shared<taraxa::FullNode>(
      context1, std::string("./core_tests/conf_taraxa1.json")));

  // node1->setVerbose(true);
  node1->setDebug(true);
  node1->start(true /*boot_node*/);

  // send package
  FullNodeConfig conf2("./core_tests/conf_taraxa2.json");
  auto nw2(std::make_shared<taraxa::Network>(conf2.network));

  std::unique_ptr<boost::asio::io_context::work> work(
      new boost::asio::io_context::work(context1));

  boost::thread t([&context1]() { context1.run(); });

  nw2->start();
  std::vector<DagBlock> blks;

  DagBlock blk1(blk_hash_t(0), {}, {}, sig_t(77777), blk_hash_t(1), addr_t(16));
  DagBlock blk2(blk_hash_t(1), {}, {}, sig_t(77777), blk_hash_t(2), addr_t(16));
  DagBlock blk3(blk_hash_t(2), {}, {}, sig_t(77777), blk_hash_t(3), addr_t(16));
  DagBlock blk4(blk_hash_t(3), {}, {}, sig_t(77777), blk_hash_t(4), addr_t(16));
  DagBlock blk5(blk_hash_t(4), {}, {}, sig_t(77777), blk_hash_t(5), addr_t(16));
  DagBlock blk6(blk_hash_t(5), {blk_hash_t(4), blk_hash_t(3)}, {}, sig_t(77777),
                blk_hash_t(6), addr_t(16));

  blks.emplace_back(blk6);
  blks.emplace_back(blk5);
  blks.emplace_back(blk4);
  blks.emplace_back(blk3);
  blks.emplace_back(blk2);
  blks.emplace_back(blk1);

  std::cout << "Waiting connection for 1000 milliseconds ..." << std::endl;
  taraxa::thisThreadSleepForMilliSeconds(1000);

  for (auto i = 0; i < blks.size(); ++i) {
    nw2->sendBlock(node1->getNetwork()->getNodeId(), blks[i], true);
  }

  taraxa::thisThreadSleepForMilliSeconds(1000);

  work.reset();
  nw2->stop();

  auto num_received_blks = node1->getNumReceivedBlocks();
  auto num_vertices_in_dag = node1->getNumVerticesInDag().first;
  for (auto i = 0; i < 10; ++i) {
    if (num_received_blks == blks.size() && num_vertices_in_dag == 7) {
      break;
    }
    taraxa::thisThreadSleepForMilliSeconds(500);
  }

  // node1->drawGraph("dot.txt");
  EXPECT_EQ(node1->getNumReceivedBlocks(), blks.size());
  EXPECT_EQ(node1->getNumVerticesInDag().first, 7);
  EXPECT_EQ(node1->getNumEdgesInDag().first, 8);

  node1->stop();
  t.join();
}

TEST(FullNode, receive_send_transaction) {
  boost::asio::io_context context1;
  FullNodeConfig conf("./core_tests/conf_taraxa1.json");
  auto node1(std::make_shared<taraxa::FullNode>(context1, conf));
  auto rpc(
      std::make_shared<taraxa::Rpc>(context1, conf.rpc, node1->getShared()));
  rpc->start();
  node1->setDebug(true);
  node1->start(true /*boot_node*/);

  std::unique_ptr<boost::asio::io_context::work> work(
      new boost::asio::io_context::work(context1));

  boost::thread t([&context1]() { context1.run(); });

  try {
    system("./core_tests/curl_send_1000_trx.sh");
  } catch (std::exception& e) {
    std::cerr << e.what() << std::endl;
  }
  std::cout << "1000 transaction are sent through RPC ..." << std::endl;

  auto num_proposed_blk = node1->getNumProposedBlocks();
  for (auto i = 0; i < 10; i++) {
    if (num_proposed_blk > 0) {
      break;
    }
    taraxa::thisThreadSleepForMilliSeconds(500);
  }

  work.reset();
  node1->stop();
  rpc->stop();
  t.join();
  EXPECT_GT(node1->getNumProposedBlocks(), 0);
}

}  // namespace taraxa

int main(int argc, char** argv) {
  dev::LoggingOptions logOptions;
  logOptions.verbosity = dev::VerbosityInfo;
  dev::setupLogging(logOptions);
  // use the in-memory db so test will not affect other each other through
  // persistent storage
  dev::db::setDatabaseKind(dev::db::DatabaseKind::MemoryDB);
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
