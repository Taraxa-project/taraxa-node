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
#include "pbft_manager.hpp"
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

TEST(Top, top_reset) {
  const char* input1[] = {"./build/main", "--conf_taraxa",
                          "./core_tests/conf/conf_taraxa1.json", "-v", "0"};
  const char* input2[] = {"./build/main2", "--conf_taraxa",
                          "./core_tests/conf/conf_taraxa2.json", "-v", "0"};
  try {
    std::cout << "Copying main2 ..." << std::endl;
    system("cp ./build/main ./build/main2");
  } catch (std::exception& e) {
    std::cerr << e.what() << std::endl;
  }

  Top top1(5, input1);
  EXPECT_TRUE(top1.isActive());
  std::cout << "Top1 created ..." << std::endl;

  Top top2(5, input2);
  EXPECT_TRUE(top2.isActive());
  std::cout << "Top2 created ..." << std::endl;
  std::cout << "Sleep for 1 second ..." << std::endl;
  taraxa::thisThreadSleepForMilliSeconds(1000);

  try {
    std::string sendtrx1 =
        R"(curl -s -d '{"action": "create_test_coin_transactions", 
                                      "secret": "3800b2875669d9b2053c1aff9224ecfdc411423aac5b5a73d7a45ced1c3b9dcd", 
                                      "delay": 5, 
                                      "number": 6000, 
                                      "seed": 1 }' 0.0.0.0:7777)";
    std::string sendtrx2 =
        R"(curl -s -d '{"action": "create_test_coin_transactions", 
                                      "secret": "e6af8ca3b4074243f9214e16ac94831f17be38810d09a3edeb56ab55be848a1e", 
                                      "delay": 7, 
                                      "number": 4000, 
                                      "seed": 2 }' 0.0.0.0:7778)";
    std::cout << "Sending trxs ..." << std::endl;
    std::thread t1([sendtrx1]() { system(sendtrx1.c_str()); });
    std::thread t2([sendtrx2]() { system(sendtrx2.c_str()); });

    t1.join();
    t2.join();
    std::cout << "All trxs sent..." << std::endl;
  } catch (std::exception& e) {
    std::cerr << e.what() << std::endl;
  }

  auto node1 = top1.getNode();
  auto node2 = top2.getNode();

  EXPECT_NE(node1, nullptr);
  EXPECT_NE(node2, nullptr);

  // check dags
  auto num_vertices1 = node1->getNumVerticesInDag();
  auto num_vertices2 = node2->getNumVerticesInDag();

  for (auto i = 0; i < 50; i++) {
    if (i % 10 == 0) {
      std::cout << "Wait for vertices syncing ..." << std::endl;
    }
    num_vertices1 = node1->getNumVerticesInDag();
    num_vertices2 = node2->getNumVerticesInDag();

    if (num_vertices1 == num_vertices2 &&
        node1->getTransactionStatusCount() == 10000 &&
        node2->getTransactionStatusCount() == 10000)
      break;
    taraxa::thisThreadSleepForMilliSeconds(500);
  }

  num_vertices1 = node1->getNumVerticesInDag();
  num_vertices2 = node2->getNumVerticesInDag();
  std::cout << "num_vertices 1 pivot: " << num_vertices1.first << std::endl;
  std::cout << "num_vertices 2 pivot: " << num_vertices2.first << std::endl;

  EXPECT_EQ(num_vertices1, num_vertices2);

  EXPECT_EQ(node1->getTransactionStatusCount(), 10000);
  EXPECT_EQ(node2->getTransactionStatusCount(), 10000);

  top2.stop();
  top1.stop();

  // ------------------------ Reset -----------------
  top1.reset();
  std::cout << "Top1 reset ...\n";
  top2.reset();
  std::cout << "Top2 reset ...\n";
  top1.start();
  top2.start();
  node1 = top1.getNode();
  node2 = top2.getNode();
  EXPECT_NE(node1, nullptr);
  EXPECT_NE(node2, nullptr);

  try {
    std::string sendtrx1 =
        R"(curl -d '{"action": "create_test_coin_transactions",
                                      "secret":
                                      "3800b2875669d9b2053c1aff9224ecfdc411423aac5b5a73d7a45ced1c3b9dcd",
                                      "delay": 5,
                                      "number": 6000,
                                      "seed": 1 }' 0.0.0.0:7777)";
    std::string sendtrx2 =
        R"(curl -d '{"action": "create_test_coin_transactions",
                                      "secret":
                                      "e6af8ca3b4074243f9214e16ac94831f17be38810d09a3edeb56ab55be848a1e",
                                      "delay": 7,
                                      "number": 4000,
                                      "seed": 2 }' 0.0.0.0:7778)";
    std::cout << "Sending trxs ..." << std::endl;
    std::thread t1([sendtrx1]() { system(sendtrx1.c_str()); });
    std::thread t2([sendtrx2]() { system(sendtrx2.c_str()); });

    t1.join();
    t2.join();
    std::cout << "All trxs sent..." << std::endl;
  } catch (std::exception& e) {
    std::cerr << e.what() << std::endl;
  }
  num_vertices1 = node1->getNumVerticesInDag();
  num_vertices2 = node2->getNumVerticesInDag();

  for (auto i = 0; i < 50; i++) {
    if (i % 10 == 0) {
      std::cout << "Wait for vertices syncing ..." << std::endl;
    }
    num_vertices1 = node1->getNumVerticesInDag();
    num_vertices2 = node2->getNumVerticesInDag();

    if (num_vertices1 == num_vertices2 &&
        node1->getTransactionStatusCount() == 10000 &&
        node2->getTransactionStatusCount() == 10000)
      break;
    taraxa::thisThreadSleepForMilliSeconds(500);
  }

  num_vertices1 = node1->getNumVerticesInDag();
  num_vertices2 = node2->getNumVerticesInDag();

  EXPECT_EQ(num_vertices1, num_vertices2);

  EXPECT_EQ(node1->getTransactionStatusCount(), 10000);
  EXPECT_EQ(node2->getTransactionStatusCount(), 10000);

  top2.kill();
  top1.kill();
  // delete main2
  try {
    std::cout << "main2 deleted ..." << std::endl;
    system("rm -f ./build/main2");
  } catch (std::exception& e) {
    std::cerr << e.what() << std::endl;
  }
}

TEST(FullNode, full_node_reset) {
  boost::asio::io_context context1;
  boost::asio::io_context context2;

  auto node1(std::make_shared<taraxa::FullNode>(
      context1, std::string("./core_tests/conf/conf_taraxa1.json")));

  node1->setDebug(true);
  node1->start(true);  // boot node

  // send package
  FullNodeConfig conf2("./core_tests/conf/conf_taraxa2.json");
  auto nw2(std::make_shared<taraxa::Network>(conf2.network));

  std::unique_ptr<boost::asio::io_context::work> work(
      new boost::asio::io_context::work(context1));

  boost::thread t([&context1]() { context1.run(); });

  nw2->start();
  std::vector<DagBlock> blks;

  DagBlock blk1(blk_hash_t(0), 0, {}, {}, sig_t(77777), blk_hash_t(1),
                addr_t(16));
  DagBlock blk2(blk_hash_t(1), 0, {}, {}, sig_t(77777), blk_hash_t(2),
                addr_t(16));
  DagBlock blk3(blk_hash_t(2), 0, {}, {}, sig_t(77777), blk_hash_t(3),
                addr_t(16));
  DagBlock blk4(blk_hash_t(3), 0, {}, {}, sig_t(77777), blk_hash_t(4),
                addr_t(16));
  DagBlock blk5(blk_hash_t(4), 0, {}, {}, sig_t(77777), blk_hash_t(5),
                addr_t(16));
  DagBlock blk6(blk_hash_t(5), 0, {blk_hash_t(4), blk_hash_t(3)}, {},
                sig_t(77777), blk_hash_t(6), addr_t(16));

  blks.emplace_back(blk6);
  blks.emplace_back(blk5);
  blks.emplace_back(blk4);
  blks.emplace_back(blk3);
  blks.emplace_back(blk2);
  blks.emplace_back(blk1);

  std::cout << "Waiting connection for 100 milliseconds ..." << std::endl;
  taraxa::thisThreadSleepForMilliSeconds(100);

  for (auto i = 0; i < blks.size(); ++i) {
    nw2->sendBlock(node1->getNetwork()->getNodeId(), blks[i], true);
  }

  taraxa::thisThreadSleepForMilliSeconds(1000);

  auto num_received_blks = node1->getNumReceivedBlocks();
  auto num_vertices_in_dag = node1->getNumVerticesInDag().first;
  for (auto i = 0; i < 10; ++i) {
    if (num_received_blks == blks.size() && num_vertices_in_dag == 7) {
      break;
    }
    taraxa::thisThreadSleepForMilliSeconds(500);
  }

  EXPECT_EQ(node1->getNumReceivedBlocks(), blks.size());
  EXPECT_EQ(node1->getNumVerticesInDag().first, 7);
  EXPECT_EQ(node1->getNumEdgesInDag().first, 8);

  node1->stop();
  node1->reset();
  node1->start(true);  // boot node
  std::cout << "Waiting connection for 100 milliseconds ..." << std::endl;
  taraxa::thisThreadSleepForMilliSeconds(100);

  for (auto i = 0; i < blks.size(); ++i) {
    nw2->sendBlock(node1->getNetwork()->getNodeId(), blks[i], true);
  }

  taraxa::thisThreadSleepForMilliSeconds(1000);

  num_received_blks = node1->getNumReceivedBlocks();
  num_vertices_in_dag = node1->getNumVerticesInDag().first;
  for (auto i = 0; i < 10; ++i) {
    if (num_received_blks == blks.size() && num_vertices_in_dag == 7) {
      break;
    }
    taraxa::thisThreadSleepForMilliSeconds(500);
  }

  work.reset();
  nw2->stop();
  node1->stop();
  t.join();
}

TEST(Top, sync_five_nodes_simple) {
  const char* input1[] = {"./build/main", "--conf_taraxa",
                          "./core_tests/conf/conf_taraxa1.json", "-v", "0"};
  const char* input2[] = {"./build/main2", "--conf_taraxa",
                          "./core_tests/conf/conf_taraxa2.json", "-v", "0"};
  const char* input3[] = {"./build/main3", "--conf_taraxa",
                          "./core_tests/conf/conf_taraxa3.json", "-v", "0"};
  const char* input4[] = {"./build/main4", "--conf_taraxa",
                          "./core_tests/conf/conf_taraxa4.json", "-v", "0"};
  const char* input5[] = {"./build/main5", "--conf_taraxa",
                          "./core_tests/conf/conf_taraxa5.json", "-v", "0"};

  // copy main2, main3, main4, main5
  try {
    std::cout << "Copying main2 ..." << std::endl;
    system("cp ./build/main ./build/main2");
    std::cout << "Copying main3 ..." << std::endl;
    system("cp ./build/main ./build/main3");
    std::cout << "Copying main4 ..." << std::endl;
    system("cp ./build/main ./build/main4");
    std::cout << "Copying main5 ..." << std::endl;
    system("cp ./build/main ./build/main5");
  } catch (std::exception& e) {
    std::cerr << e.what() << std::endl;
  }

  Top top1(5, input1);
  EXPECT_TRUE(top1.isActive());
  taraxa::thisThreadSleepForMilliSeconds(1000);
  std::cout << "Top1 created ..." << std::endl;

  Top top2(5, input2);
  EXPECT_TRUE(top2.isActive());
  std::cout << "Top2 created ..." << std::endl;

  Top top3(5, input3);
  EXPECT_TRUE(top3.isActive());
  std::cout << "Top3 created ..." << std::endl;

  Top top4(5, input4);
  EXPECT_TRUE(top4.isActive());
  std::cout << "Top4 created ..." << std::endl;

  Top top5(5, input5);
  EXPECT_TRUE(top5.isActive());
  std::cout << "Top5 created ..." << std::endl;
  // wait for top2, top3, top4, top5 initialize
  taraxa::thisThreadSleepForMilliSeconds(2000);

  // send 1000 trxs
  try {
    std::string sendtrx1 =
        R"(curl -s -d '{"action": "create_test_coin_transactions", 
                                      "secret": "3800b2875669d9b2053c1aff9224ecfdc411423aac5b5a73d7a45ced1c3b9dcd", 
                                      "delay": 5, 
                                      "number": 6000, 
                                      "seed": 1 }' 0.0.0.0:7777)";
    std::string sendtrx2 =
        R"(curl -s -d '{"action": "create_test_coin_transactions", 
                                      "secret": "e6af8ca3b4074243f9214e16ac94831f17be38810d09a3edeb56ab55be848a1e", 
                                      "delay": 7, 
                                      "number": 4000, 
                                      "seed": 2 }' 0.0.0.0:7778)";
    std::string sendtrx3 =
        R"(curl -s -d '{"action": "create_test_coin_transactions", 
                                      "secret": "f1261c9f09b0b483486c3b298f7c1ee001ff37e10023596528af93e34ba13f5f", 
                                      "delay": 3, 
                                      "number": 3000, 
                                      "seed": 3 }' 0.0.0.0:7779)";
    std::string sendtrx4 =
        R"(curl -s -d '{"action": "create_test_coin_transactions", 
                                      "secret": "7f38ee36812f2e4b1d75c9d21057fd718b9e7903ee9f9d4eb93b690790bb4029", 
                                      "delay": 10, 
                                      "number": 3000, 
                                      "seed": 4 }' 0.0.0.0:7780)";
    std::string sendtrx5 =
        R"(curl -s -d '{"action": "create_test_coin_transactions", 
                                      "secret": "beb2ed10f80e3feaf971614b2674c7de01cfd3127faa1bd055ed50baa1ce34fe", 
                                      "delay": 23, 
                                      "number": 4000, 
                                      "seed": 5 }' 0.0.0.0:7781)";
    std::cout << "Sending trxs ..." << std::endl;
    std::thread t1([sendtrx1]() { system(sendtrx1.c_str()); });
    std::thread t2([sendtrx2]() { system(sendtrx2.c_str()); });
    std::thread t3([sendtrx3]() { system(sendtrx3.c_str()); });
    std::thread t4([sendtrx4]() { system(sendtrx4.c_str()); });
    std::thread t5([sendtrx5]() { system(sendtrx5.c_str()); });

    t1.join();
    t2.join();
    t3.join();
    t4.join();
    t5.join();
    std::cout << "All trxs sent..." << std::endl;

  } catch (std::exception& e) {
    std::cerr << e.what() << std::endl;
  }

  auto node1 = top1.getNode();
  auto node2 = top2.getNode();
  auto node3 = top3.getNode();
  auto node4 = top4.getNode();
  auto node5 = top5.getNode();

  EXPECT_NE(node1, nullptr);
  EXPECT_NE(node2, nullptr);
  EXPECT_NE(node3, nullptr);
  EXPECT_NE(node4, nullptr);
  EXPECT_NE(node5, nullptr);

  auto num_peers1 = node1->getPeerCount();
  auto num_peers2 = node2->getPeerCount();
  auto num_peers3 = node3->getPeerCount();
  auto num_peers4 = node4->getPeerCount();
  auto num_peers5 = node5->getPeerCount();

  for (auto i = 0; i < 180; i++) {
    if (i % 10 == 0) {
      std::cout << "Wait for peers syncing ..." << std::endl;
    }
    num_peers1 = node1->getPeerCount();
    num_peers2 = node2->getPeerCount();
    num_peers3 = node3->getPeerCount();
    num_peers4 = node4->getPeerCount();
    num_peers5 = node5->getPeerCount();

    if (num_peers1 == 4 && num_peers2 == 4 && num_peers3 == 4 &&
        num_peers4 == 4 && num_peers5 == 4)
      break;
    taraxa::thisThreadSleepForMilliSeconds(500);
  }
  EXPECT_EQ(num_peers1, 4);
  EXPECT_EQ(num_peers2, 4);
  EXPECT_EQ(num_peers3, 4);
  EXPECT_EQ(num_peers4, 4);
  EXPECT_EQ(num_peers5, 4);

  // check dags
  auto num_vertices1 = node1->getNumVerticesInDag();
  auto num_vertices2 = node2->getNumVerticesInDag();
  auto num_vertices3 = node3->getNumVerticesInDag();
  auto num_vertices4 = node4->getNumVerticesInDag();
  auto num_vertices5 = node5->getNumVerticesInDag();

  for (auto i = 0; i < 180; i++) {
    if (i % 10 == 0) {
      std::cout << "Wait for vertices syncing ..." << std::endl;
    }
    num_vertices1 = node1->getNumVerticesInDag();
    num_vertices2 = node2->getNumVerticesInDag();
    num_vertices3 = node3->getNumVerticesInDag();
    num_vertices4 = node4->getNumVerticesInDag();
    num_vertices5 = node5->getNumVerticesInDag();

    if (num_vertices1 == num_vertices2 && num_vertices2 == num_vertices3 &&
        num_vertices3 == num_vertices4 && num_vertices4 == num_vertices5 &&
        node1->getTransactionStatusCount() == 20000 &&
        node2->getTransactionStatusCount() == 20000 &&
        node3->getTransactionStatusCount() == 20000 &&
        node4->getTransactionStatusCount() == 20000 &&
        node5->getTransactionStatusCount() == 20000)
      break;
    taraxa::thisThreadSleepForMilliSeconds(500);
  }

  num_vertices1 = node1->getNumVerticesInDag();
  num_vertices2 = node2->getNumVerticesInDag();
  num_vertices3 = node3->getNumVerticesInDag();
  num_vertices4 = node4->getNumVerticesInDag();
  num_vertices5 = node5->getNumVerticesInDag();

  EXPECT_EQ(num_vertices1, num_vertices2);
  EXPECT_EQ(num_vertices2, num_vertices3);
  EXPECT_EQ(num_vertices3, num_vertices4);
  EXPECT_EQ(num_vertices4, num_vertices5);

  EXPECT_EQ(node1->getTransactionStatusCount(), 20000);
  EXPECT_EQ(node2->getTransactionStatusCount(), 20000);
  EXPECT_EQ(node3->getTransactionStatusCount(), 20000);
  EXPECT_EQ(node4->getTransactionStatusCount(), 20000);
  EXPECT_EQ(node5->getTransactionStatusCount(), 20000);

  EXPECT_GT(node1->getNumProposedBlocks(), 2);
  EXPECT_GT(node2->getNumProposedBlocks(), 2);
  EXPECT_GT(node3->getNumProposedBlocks(), 2);
  EXPECT_GT(node4->getNumProposedBlocks(), 2);
  EXPECT_GT(node5->getNumProposedBlocks(), 2);

  top5.kill();
  top4.kill();
  top3.kill();
  top2.kill();
  top1.kill();
  // delete main2
  try {
    std::cout << "main5 deleted ..." << std::endl;
    system("rm -f ./build/main5");
    std::cout << "main4 deleted ..." << std::endl;
    system("rm -f ./build/main4");
    std::cout << "main3 deleted ..." << std::endl;
    system("rm -f ./build/main3");
    std::cout << "main2 deleted ..." << std::endl;
    system("rm -f ./build/main2");
  } catch (std::exception& e) {
    std::cerr << e.what() << std::endl;
  }
}

TEST(FullNode, insert_anchor_and_compute_order) {
  boost::asio::io_context context;
  auto node(std::make_shared<taraxa::FullNode>(
      context, std::string("./core_tests/conf/conf_taraxa1.json")));
  node->start(true);  // boot node

  auto num_blks = g_mock_dag0.size();
  for (int i = 1; i <= 9; i++) {
    node->insertBlock(g_mock_dag0[i]);
  }
  taraxa::thisThreadSleepForMilliSeconds(200);
  std::string pivot;
  std::vector<std::string> tips;

  // -------- first period ----------

  node->getLatestPivotAndTips(pivot, tips);
  uint64_t period;
  std::shared_ptr<vec_blk_t> order;
  std::tie(period, order) = node->getDagBlockOrder(blk_hash_t(pivot));
  EXPECT_EQ(period, 1);
  EXPECT_EQ(order->size(), 6);

  if (order->size() == 6) {
    EXPECT_EQ((*order)[0], blk_hash_t(3));
    EXPECT_EQ((*order)[1], blk_hash_t(6));
    EXPECT_EQ((*order)[2], blk_hash_t(2));
    EXPECT_EQ((*order)[3], blk_hash_t(1));
    EXPECT_EQ((*order)[4], blk_hash_t(5));
    EXPECT_EQ((*order)[5], blk_hash_t(7));
  }
  node->setDagBlockOrder(blk_hash_t(pivot), period);

  // -------- second period ----------

  for (int i = 10; i <= 16; i++) {
    node->insertBlock(g_mock_dag0[i]);
  }
  taraxa::thisThreadSleepForMilliSeconds(200);

  node->getLatestPivotAndTips(pivot, tips);
  std::tie(period, order) = node->getDagBlockOrder(blk_hash_t(pivot));
  EXPECT_EQ(period, 2);
  if (order->size() == 7) {
    EXPECT_EQ((*order)[0], blk_hash_t(11));
    EXPECT_EQ((*order)[1], blk_hash_t(10));
    EXPECT_EQ((*order)[2], blk_hash_t(13));
    EXPECT_EQ((*order)[3], blk_hash_t(9));
    EXPECT_EQ((*order)[4], blk_hash_t(12));
    EXPECT_EQ((*order)[5], blk_hash_t(14));
    EXPECT_EQ((*order)[6], blk_hash_t(15));
  }
  node->setDagBlockOrder(blk_hash_t(pivot), period);

  // -------- third period ----------

  for (int i = 17; i < g_mock_dag0.size(); i++) {
    node->insertBlock(g_mock_dag0[i]);
  }
  taraxa::thisThreadSleepForMilliSeconds(200);

  node->getLatestPivotAndTips(pivot, tips);
  std::tie(period, order) = node->getDagBlockOrder(blk_hash_t(pivot));
  EXPECT_EQ(period, 3);
  if (order->size() == 5) {
    EXPECT_EQ((*order)[0], blk_hash_t(17));
    EXPECT_EQ((*order)[1], blk_hash_t(16));
    EXPECT_EQ((*order)[2], blk_hash_t(8));
    EXPECT_EQ((*order)[3], blk_hash_t(18));
    EXPECT_EQ((*order)[4], blk_hash_t(19));
  }
  node->setDagBlockOrder(blk_hash_t(pivot), period);
  node->stop();
}

TEST(Top, create_top_level_db) {
  {
    dev::db::setDatabaseKind(dev::db::DatabaseKind::LevelDB);

    const char* inputs[] = {"./build/main", "--conf_taraxa",
                            "./core_tests/conf/conf_taraxa1.json", "-v", "0"};
    Top top(5, inputs);
    taraxa::thisThreadSleepForSeconds(1);
    EXPECT_TRUE(top.isActive());
    top.stop();
    EXPECT_FALSE(top.isActive());
    top.start(5, inputs);
    EXPECT_TRUE(top.isActive());
    top.kill();
    EXPECT_FALSE(top.isActive());
  }
  dev::db::setDatabaseKind(dev::db::DatabaseKind::MemoryDB);
}

TEST(Top, create_top_memory_db) {
  {
    const char* inputs[] = {"./build/main", "--conf_taraxa",
                            "./core_tests/conf/conf_taraxa1.json", "-v", "0"};
    Top top(5, inputs);
    taraxa::thisThreadSleepForSeconds(1);
    EXPECT_TRUE(top.isActive());
    top.stop();
    EXPECT_FALSE(top.isActive());
    top.start(5, inputs);
    EXPECT_TRUE(top.isActive());
    top.kill();
    EXPECT_FALSE(top.isActive());
  }
}

TEST(Top, reconstruct_dag) {
  dev::db::setDatabaseKind(dev::db::DatabaseKind::LevelDB);
  unsigned long vertices1 = 0;
  unsigned long vertices2 = 0;
  unsigned long vertices3 = 0;
  unsigned long vertices4 = 0;

  auto num_blks = g_mock_dag0.size();
  {
    boost::asio::io_context context;
    FullNodeConfig conf("./core_tests/conf/conf_taraxa1.json");
    auto node(
        std::make_shared<taraxa::FullNode>(context, conf, true));  // destroy DB

    node->start(false);
    taraxa::thisThreadSleepForMilliSeconds(500);

    for (int i = 1; i < num_blks; i++) {
      node->insertBlock(g_mock_dag0[i]);
    }

    taraxa::thisThreadSleepForMilliSeconds(500);
    vertices1 = node->getNumVerticesInDag().first;
    EXPECT_EQ(vertices1, num_blks);
    node->stop();
  }
  {
    boost::asio::io_context context;
    FullNodeConfig conf("./core_tests/conf/conf_taraxa1.json");
    auto node(std::make_shared<taraxa::FullNode>(context, conf,
                                                 false));  // no destroy DB

    node->start(false);
    taraxa::thisThreadSleepForMilliSeconds(500);

    vertices2 = node->getNumVerticesInDag().first;
    EXPECT_EQ(vertices2, num_blks);
    node->stop();
    std::cout << "Delete DB ..." << std::endl;
    node->destroyDB();
    std::cout << "DB deleted ..." << std::endl;

    node->start(false);
    for (int i = 1; i < num_blks; i++) {
      node->insertBlock(g_mock_dag0[i]);
    }

    vertices3 = node->getNumVerticesInDag().first;
    node->stop();

    node->start(false);
    vertices4 = node->getNumVerticesInDag().first;
    node->stop();
  }
  EXPECT_EQ(vertices1, vertices2);
  EXPECT_EQ(vertices2, vertices3);
  EXPECT_EQ(vertices3, vertices4);

  dev::db::setDatabaseKind(dev::db::DatabaseKind::MemoryDB);
}

TEST(Top, sync_two_nodes1) {
  const char* input1[] = {"./build/main",
                          "--conf_taraxa",
                          "./core_tests/conf/conf_taraxa1.json",
                          "-v",
                          "0",
                          "--destroy_db"};

  Top top1(6, input1);
  EXPECT_TRUE(top1.isActive());
  std::cout << "Top1 created ..." << std::endl;

  thisThreadSleepForMilliSeconds(500);

  // copy main2
  try {
    std::cout << "Copying main2 ..." << std::endl;
    system("cp ./build/main ./build/main2");
  } catch (std::exception& e) {
    std::cerr << e.what() << std::endl;
  }
  top1.stop();
  top1.start(6, input1);
  taraxa::thisThreadSleepForMilliSeconds(500);

  const char* input2[] = {"./build/main2",
                          "--conf_taraxa",
                          "./core_tests/conf/conf_taraxa2.json",
                          "-v",
                          "0",
                          "--destroy_db"};
  Top top2(6, input2);
  EXPECT_TRUE(top2.isActive());
  std::cout << "Top2 created ..." << std::endl;
  // wait for top2 initialize
  taraxa::thisThreadSleepForMilliSeconds(1000);

  // send 1000 trxs
  try {
    std::cout << "Sending 1000 trxs ..." << std::endl;
    system("./core_tests/scripts/curl_send_1000_trx.sh");
    std::cout << "1000 trxs sent ..." << std::endl;

  } catch (std::exception& e) {
    std::cerr << e.what() << std::endl;
  }

  auto node1 = top1.getNode();
  auto node2 = top2.getNode();
  EXPECT_NE(node1, nullptr);
  EXPECT_NE(node2, nullptr);
  auto vertices1 = node1->getNumVerticesInDag();
  auto vertices2 = node2->getNumVerticesInDag();
  // add more delay if sync is not done
  for (auto i = 0; i < 60; i++) {
    if (vertices1 == vertices2 && vertices1.first > 3) break;
    taraxa::thisThreadSleepForMilliSeconds(500);
    vertices1 = node1->getNumVerticesInDag();
    vertices2 = node2->getNumVerticesInDag();
  }
  EXPECT_GT(vertices1.first, 3);
  EXPECT_GT(vertices1.second, 3);
  EXPECT_EQ(vertices1, vertices2);

  top2.kill();
  top1.kill();
  // delete main2
  try {
    std::cout << "main2 deleted ..." << std::endl;
    system("rm -f ./build/main2");
  } catch (std::exception& e) {
    std::cerr << e.what() << std::endl;
  }
}

TEST(Top, sync_two_nodes2) {
  const char* input1[] = {"./build/main",
                          "--conf_taraxa",
                          "./core_tests/conf/conf_taraxa1.json",
                          "-v",
                          "0",
                          "--destroy_db"};

  Top top1(6, input1);
  EXPECT_TRUE(top1.isActive());
  std::cout << "Top1 created ..." << std::endl;

  thisThreadSleepForMilliSeconds(500);

  // send 1000 trxs
  try {
    std::cout << "Sending 1000 trxs ..." << std::endl;
    system("./core_tests/scripts/curl_send_1000_trx.sh");
    std::cout << "1000 trxs sent ..." << std::endl;

  } catch (std::exception& e) {
    std::cerr << e.what() << std::endl;
  }

  top1.stop();
  top1.start(6, input1);
  taraxa::thisThreadSleepForMilliSeconds(500);

  // copy main2
  try {
    std::cout << "Copying main2 ..." << std::endl;
    system("cp ./build/main ./build/main2");
  } catch (std::exception& e) {
    std::cerr << e.what() << std::endl;
  }

  const char* input2[] = {"./build/main2",
                          "--conf_taraxa",
                          "./core_tests/conf/conf_taraxa2.json",
                          "-v",
                          "0",
                          "--destroy_db"};
  Top top2(6, input2);
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
  // let node2 sync node1
  for (auto i = 0; i < 60; i++) {
    if (vertices1 == vertices2 && vertices1.first > 3) break;
    taraxa::thisThreadSleepForMilliSeconds(500);
    vertices1 = node1->getNumVerticesInDag();
    vertices2 = node2->getNumVerticesInDag();
  }
  EXPECT_GT(vertices1.first, 3);
  EXPECT_GT(vertices1.second, 3);
  EXPECT_EQ(vertices1, vertices2);
  top2.kill();
  top1.kill();
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
      context, std::string("./core_tests/conf/conf_taraxa1.json")));
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
      context, std::string("./core_tests/conf/conf_taraxa1.json")));
  node->start(true);  // boot node
  addr_t acc1 = node->getAddress();

  bal_t initbal(10000000);
  node->setBalance(acc1, initbal);
  auto res = node->getBalance(acc1);
  EXPECT_TRUE(res.second);
  EXPECT_EQ(res.first, initbal);
  for (auto const& t : g_trx_signed_samples) {
    node->insertTransaction(t);
    taraxa::thisThreadSleepForMilliSeconds(50);
  }

  taraxa::thisThreadSleepForMilliSeconds(3000);

  EXPECT_GT(node->getNumProposedBlocks(), 0);

  // The test will form a single chain
  std::vector<std::string> ghost;
  node->getGhostPath(Dag::GENESIS, ghost);
  vec_blk_t blks;
  std::vector<std::vector<uint>> modes;
  EXPECT_GT(ghost.size(), 1);
  uint64_t period = 0, cur_period, cur_period2;
  std::shared_ptr<vec_blk_t> order;
  std::shared_ptr<PbftManager> pbft_mgr = node->getPbftManager();
  // create a period for every 2 pivots
  for (int i = 0; i < ghost.size(); i += 2) {
    auto anchor = blk_hash_t(ghost[i]);
    std::tie(cur_period, order) = node->getDagBlockOrder(anchor);
    // call twice should not change states
    std::tie(cur_period2, order) = node->getDagBlockOrder(anchor);
    EXPECT_EQ(cur_period, cur_period2);
    EXPECT_EQ(cur_period, ++period);
    auto sche = node->createMockTrxSchedule(order);
    EXPECT_NE(sche, nullptr);
    // if (!sche) continue;
    ScheduleBlock sche_blk(blk_hash_t(100), 12345, *sche);
    // set period
    node->setDagBlockOrder(anchor, cur_period);
    bool ret = node->executeScheduleBlock(
        sche_blk, pbft_mgr->sortition_account_balance_table);
    EXPECT_TRUE(ret);
    taraxa::thisThreadSleepForMilliSeconds(200);
  }
  // pickup the last period when dag (chain) size is odd number
  if (ghost.size() % 2 == 0) {
    std::tie(cur_period, order) =
        node->getDagBlockOrder(blk_hash_t(ghost.back()));
    EXPECT_EQ(cur_period, ++period);
    auto sche = node->createMockTrxSchedule(order);
    ScheduleBlock sche_blk(blk_hash_t(100), 12345, *sche);
    bool ret = node->executeScheduleBlock(
        sche_blk, pbft_mgr->sortition_account_balance_table);
    EXPECT_TRUE(ret);
    taraxa::thisThreadSleepForMilliSeconds(200);
  }

  node->stop();
  auto coin_distributed = 0;
  for (auto const& t : g_trx_signed_samples) {
    auto res = node->getBalance(t.getReceiver());
    EXPECT_TRUE(res.second);
    EXPECT_EQ(res.first, t.getValue());
    coin_distributed += res.first;
  }
  res = node->getBalance(acc1);
  EXPECT_EQ(res.first, initbal - coin_distributed);
}

TEST(FullNode, send_and_receive_out_order_messages) {
  boost::asio::io_context context1;
  boost::asio::io_context context2;

  auto node1(std::make_shared<taraxa::FullNode>(
      context1, std::string("./core_tests/conf/conf_taraxa1.json")));

  // node1->setVerbose(true);
  node1->setDebug(true);
  node1->start(true);  // boot node

  // send package
  FullNodeConfig conf2("./core_tests/conf/conf_taraxa2.json");
  auto nw2(std::make_shared<taraxa::Network>(conf2.network));

  std::unique_ptr<boost::asio::io_context::work> work(
      new boost::asio::io_context::work(context1));

  boost::thread t([&context1]() { context1.run(); });

  nw2->start();
  std::vector<DagBlock> blks;

  DagBlock blk1(blk_hash_t(0), 1, {}, {}, sig_t(77777), blk_hash_t(1),
                addr_t(16));
  DagBlock blk2(blk_hash_t(1), 2, {}, {}, sig_t(77777), blk_hash_t(2),
                addr_t(16));
  DagBlock blk3(blk_hash_t(2), 3, {}, {}, sig_t(77777), blk_hash_t(3),
                addr_t(16));
  DagBlock blk4(blk_hash_t(3), 4, {}, {}, sig_t(77777), blk_hash_t(4),
                addr_t(16));
  DagBlock blk5(blk_hash_t(4), 5, {}, {}, sig_t(77777), blk_hash_t(5),
                addr_t(16));
  DagBlock blk6(blk_hash_t(5), 6, {blk_hash_t(4), blk_hash_t(3)}, {},
                sig_t(77777), blk_hash_t(6), addr_t(16));

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

TEST(FullNode, save_network_to_file) {
  {
    boost::asio::io_context context1;
    FullNodeConfig conf1("./core_tests/conf/conf_taraxa1.json");
    auto node1(std::make_shared<taraxa::FullNode>(context1, conf1));
    boost::asio::io_context context2;
    FullNodeConfig conf2("./core_tests/conf/conf_taraxa2.json");
    auto node2(std::make_shared<taraxa::FullNode>(context2, conf2));
    boost::asio::io_context context3;
    FullNodeConfig conf3("./core_tests/conf/conf_taraxa3.json");
    auto node3(std::make_shared<taraxa::FullNode>(context3, conf3));

    node1->start(true);
    node2->start(false);
    node3->start(false);

    for (int i = 0; i < 30; i++) {
      taraxa::thisThreadSleepForSeconds(1);
      if (2 == node1->getPeerCount() && 2 == node2->getPeerCount() &&
          2 == node3->getPeerCount())
        break;
    }

    ASSERT_EQ(2, node1->getPeerCount());
    ASSERT_EQ(2, node2->getPeerCount());
    ASSERT_EQ(2, node3->getPeerCount());
    node1->stop();
    node2->stop();
    node3->stop();
  }
  {
    boost::asio::io_context context2;
    FullNodeConfig conf2("./core_tests/conf/conf_taraxa2.json");
    auto node2(std::make_shared<taraxa::FullNode>(context2, conf2));
    boost::asio::io_context context3;
    FullNodeConfig conf3("./core_tests/conf/conf_taraxa3.json");
    auto node3(std::make_shared<taraxa::FullNode>(context3, conf3));

    node2->start(false);
    node3->start(false);

    for (int i = 0; i < 30; i++) {
      taraxa::thisThreadSleepForSeconds(1);
      if (1 == node2->getPeerCount() && 1 == node3->getPeerCount()) break;
    }

    ASSERT_EQ(1, node2->getPeerCount());
    ASSERT_EQ(1, node3->getPeerCount());
    node2->stop();
    node3->stop();
  }
}

TEST(FullNode, receive_send_transaction) {
  boost::asio::io_context context1;
  FullNodeConfig conf("./core_tests/conf/conf_taraxa1.json");
  auto node1(std::make_shared<taraxa::FullNode>(context1, conf));
  auto rpc(
      std::make_shared<taraxa::Rpc>(context1, conf.rpc, node1->getShared()));
  rpc->start();
  node1->setDebug(true);
  node1->start(true);  // boot node

  std::unique_ptr<boost::asio::io_context::work> work(
      new boost::asio::io_context::work(context1));

  boost::thread t([&context1]() { context1.run(); });

  try {
    system("./core_tests/scripts/curl_send_1000_trx.sh");
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

TEST(FullNode, sortition_propose_one_node) {
  boost::asio::io_context context1;
  FullNodeConfig conf("./core_tests/conf/conf_taraxa1.json");
  conf.test_params.block_proposer = {1, 1, 100, 4294967295};  // 2^32

  auto node1(std::make_shared<taraxa::FullNode>(context1, conf));
  auto rpc(
      std::make_shared<taraxa::Rpc>(context1, conf.rpc, node1->getShared()));
  rpc->start();
  node1->setDebug(true);
  // destroy db !!
  node1->destroyDB();
  node1->start(true);  // boot node
  auto addr = node1->getAddress();
  bal_t init_bal = 100000000;
  node1->setBalance(addr, init_bal);
  auto res = node1->getBalance(addr);
  EXPECT_TRUE(res.second);
  EXPECT_EQ(res.first, init_bal);
  node1->setBlockProposeThresholdBeta(2048);
  std::unique_ptr<boost::asio::io_context::work> work(
      new boost::asio::io_context::work(context1));

  boost::thread t([&context1]() { context1.run(); });

  try {
    system("./core_tests/scripts/curl_send_1000_trx.sh");
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
  EXPECT_GT(node1->getNumProposedBlocks(), 5);
}

TEST(Top, sortition_propose_five_nodes) {
  const char* input1[] = {
      "./build/main",
      "--conf_taraxa",
      "./core_tests/conf/sortition_propose_block/conf_taraxa1.json",
      "-v",
      "0",
      "--destroy_db"};
  const char* input2[] = {
      "./build/main2",
      "--conf_taraxa",
      "./core_tests/conf/sortition_propose_block/conf_taraxa2.json",
      "-v",
      "0",
      "--destroy_db"};
  const char* input3[] = {
      "./build/main3",
      "--conf_taraxa",
      "./core_tests/conf/sortition_propose_block/conf_taraxa3.json",
      "-v",
      "0",
      "--destroy_db"};
  const char* input4[] = {
      "./build/main4",
      "--conf_taraxa",
      "./core_tests/conf/sortition_propose_block/conf_taraxa4.json",
      "-v",
      "0",
      "--destroy_db"};
  const char* input5[] = {
      "./build/main5",
      "--conf_taraxa",
      "./core_tests/conf/sortition_propose_block/conf_taraxa5.json",
      "-v",
      "0",
      "--destroy_db"};

  // copy main2, main3, main4, main5
  try {
    std::cout << "Copying main2 ..." << std::endl;
    system("cp ./build/main ./build/main2");
    std::cout << "Copying main3 ..." << std::endl;
    system("cp ./build/main ./build/main3");
    std::cout << "Copying main4 ..." << std::endl;
    system("cp ./build/main ./build/main4");
    std::cout << "Copying main5 ..." << std::endl;
    system("cp ./build/main ./build/main5");
  } catch (std::exception& e) {
    std::cerr << e.what() << std::endl;
  }

  Top top1(6, input1);
  EXPECT_TRUE(top1.isActive());
  taraxa::thisThreadSleepForMilliSeconds(1000);
  std::cout << "Top1 created ..." << std::endl;

  Top top2(6, input2);
  EXPECT_TRUE(top2.isActive());
  std::cout << "Top2 created ..." << std::endl;

  Top top3(6, input3);
  EXPECT_TRUE(top3.isActive());
  std::cout << "Top3 created ..." << std::endl;

  Top top4(6, input4);
  EXPECT_TRUE(top4.isActive());
  std::cout << "Top4 created ..." << std::endl;

  Top top5(6, input5);
  EXPECT_TRUE(top5.isActive());
  std::cout << "Top5 created ..." << std::endl;
  // wait for top2, top3, top4, top5 initialize
  taraxa::thisThreadSleepForMilliSeconds(2000);

  auto node1 = top1.getNode();
  auto node2 = top2.getNode();
  auto node3 = top3.getNode();
  auto node4 = top4.getNode();
  auto node5 = top5.getNode();

  // set balance
  bal_t bal(100000000);
  node1->setBalance(node1->getAddress(), bal);
  node2->setBalance(node2->getAddress(), bal);
  node3->setBalance(node3->getAddress(), bal);
  node4->setBalance(node4->getAddress(), bal);

  // send 1000 trxs
  try {
    std::string sendtrx1 =
        R"(curl -s -d '{"action": "create_test_coin_transactions", 
                                      "secret": "3800b2875669d9b2053c1aff9224ecfdc411423aac5b5a73d7a45ced1c3b9dcd", 
                                      "delay": 5, 
                                      "number": 6000, 
                                      "seed": 1 }' 0.0.0.0:7777)";
    std::string sendtrx2 =
        R"(curl -s -d '{"action": "create_test_coin_transactions", 
                                      "secret": "e6af8ca3b4074243f9214e16ac94831f17be38810d09a3edeb56ab55be848a1e", 
                                      "delay": 7, 
                                      "number": 4000, 
                                      "seed": 2 }' 0.0.0.0:7778)";
    std::string sendtrx3 =
        R"(curl -s -d '{"action": "create_test_coin_transactions", 
                                      "secret": "f1261c9f09b0b483486c3b298f7c1ee001ff37e10023596528af93e34ba13f5f", 
                                      "delay": 3, 
                                      "number": 3000, 
                                      "seed": 3 }' 0.0.0.0:7779)";
    std::string sendtrx4 =
        R"(curl -s -d '{"action": "create_test_coin_transactions", 
                                      "secret": "7f38ee36812f2e4b1d75c9d21057fd718b9e7903ee9f9d4eb93b690790bb4029", 
                                      "delay": 10, 
                                      "number": 3000, 
                                      "seed": 4 }' 0.0.0.0:7780)";
    std::string sendtrx5 =
        R"(curl -s -d '{"action": "create_test_coin_transactions", 
                                      "secret": "beb2ed10f80e3feaf971614b2674c7de01cfd3127faa1bd055ed50baa1ce34fe", 
                                      "delay": 23, 
                                      "number": 4000, 
                                      "seed": 5 }' 0.0.0.0:7781)";
    std::cout << "Sending trxs ..." << std::endl;
    std::thread t1([sendtrx1]() { system(sendtrx1.c_str()); });
    std::thread t2([sendtrx2]() { system(sendtrx2.c_str()); });
    std::thread t3([sendtrx3]() { system(sendtrx3.c_str()); });
    std::thread t4([sendtrx4]() { system(sendtrx4.c_str()); });
    std::thread t5([sendtrx5]() { system(sendtrx5.c_str()); });

    t1.join();
    t2.join();
    t3.join();
    t4.join();
    t5.join();
    std::cout << "All trxs sent..." << std::endl;

  } catch (std::exception& e) {
    std::cerr << e.what() << std::endl;
  }

  EXPECT_NE(node1, nullptr);
  EXPECT_NE(node2, nullptr);
  EXPECT_NE(node3, nullptr);
  EXPECT_NE(node4, nullptr);
  EXPECT_NE(node5, nullptr);

  auto num_peers1 = node1->getPeerCount();
  auto num_peers2 = node2->getPeerCount();
  auto num_peers3 = node3->getPeerCount();
  auto num_peers4 = node4->getPeerCount();
  auto num_peers5 = node5->getPeerCount();

  for (auto i = 0; i < 180; i++) {
    if (i % 10 == 0) {
      std::cout << "Wait for peers syncing ..." << std::endl;
    }
    num_peers1 = node1->getPeerCount();
    num_peers2 = node2->getPeerCount();
    num_peers3 = node3->getPeerCount();
    num_peers4 = node4->getPeerCount();
    num_peers5 = node5->getPeerCount();

    if (num_peers1 == 4 && num_peers2 == 4 && num_peers3 == 4 &&
        num_peers4 == 4 && num_peers5 == 4)
      break;
    taraxa::thisThreadSleepForMilliSeconds(500);
  }
  EXPECT_EQ(num_peers1, 4);
  EXPECT_EQ(num_peers2, 4);
  EXPECT_EQ(num_peers3, 4);
  EXPECT_EQ(num_peers4, 4);
  EXPECT_EQ(num_peers5, 4);

  // check dags
  auto num_vertices1 = node1->getNumVerticesInDag();
  auto num_vertices2 = node2->getNumVerticesInDag();
  auto num_vertices3 = node3->getNumVerticesInDag();
  auto num_vertices4 = node4->getNumVerticesInDag();
  auto num_vertices5 = node5->getNumVerticesInDag();

  for (auto i = 0; i < 180; i++) {
    if (i % 10 == 0) {
      std::cout << "Wait for vertices syncing ..." << std::endl;
    }
    num_vertices1 = node1->getNumVerticesInDag();
    num_vertices2 = node2->getNumVerticesInDag();
    num_vertices3 = node3->getNumVerticesInDag();
    num_vertices4 = node4->getNumVerticesInDag();
    num_vertices5 = node5->getNumVerticesInDag();

    if (num_vertices1 == num_vertices2 && num_vertices2 == num_vertices3 &&
        num_vertices3 == num_vertices4 && num_vertices4 == num_vertices5 &&
        node1->getTransactionStatusCount() == 20000 &&
        node2->getTransactionStatusCount() == 20000 &&
        node3->getTransactionStatusCount() == 20000 &&
        node4->getTransactionStatusCount() == 20000 &&
        node5->getTransactionStatusCount() == 20000)
      break;
    taraxa::thisThreadSleepForMilliSeconds(500);
  }

  num_vertices1 = node1->getNumVerticesInDag();
  num_vertices2 = node2->getNumVerticesInDag();
  num_vertices3 = node3->getNumVerticesInDag();
  num_vertices4 = node4->getNumVerticesInDag();
  num_vertices5 = node5->getNumVerticesInDag();

  EXPECT_EQ(num_vertices1, num_vertices2);
  EXPECT_EQ(num_vertices2, num_vertices3);
  EXPECT_EQ(num_vertices3, num_vertices4);
  EXPECT_EQ(num_vertices4, num_vertices5);

  EXPECT_EQ(node1->getTransactionStatusCount(), 20000);
  EXPECT_EQ(node2->getTransactionStatusCount(), 20000);
  EXPECT_EQ(node3->getTransactionStatusCount(), 20000);
  EXPECT_EQ(node4->getTransactionStatusCount(), 20000);
  EXPECT_EQ(node5->getTransactionStatusCount(), 20000);

  // num_proposed_block is static, in the case, all nodes has
  // same num proposed blocks
  EXPECT_GT(node1->getNumProposedBlocks(), 1);

  top5.kill();
  top4.kill();
  top3.kill();
  top2.kill();
  top1.kill();
  // delete main2
  try {
    std::cout << "main5 deleted ..." << std::endl;
    system("rm -f ./build/main5");
    std::cout << "main4 deleted ..." << std::endl;
    system("rm -f ./build/main4");
    std::cout << "main3 deleted ..." << std::endl;
    system("rm -f ./build/main3");
    std::cout << "main2 deleted ..." << std::endl;
    system("rm -f ./build/main2");
  } catch (std::exception& e) {
    std::cerr << e.what() << std::endl;
  }
}

}  // namespace taraxa

int main(int argc, char** argv) {
  TaraxaStackTrace st;
  dev::LoggingOptions logOptions;
  logOptions.verbosity = dev::VerbosityWarning;
  logOptions.includeChannels.push_back("DAGMGR");
  logOptions.includeChannels.push_back("EXETOR");
  logOptions.includeChannels.push_back("BLK_PP");
  logOptions.includeChannels.push_back("PR_MDL");

  dev::setupLogging(logOptions);
  // use the in-memory db so test will not affect other each other through
  // persistent storage
  dev::db::setDatabaseKind(dev::db::DatabaseKind::MemoryDB);
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
