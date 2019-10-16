
#include "full_node.hpp"
#include <gtest/gtest.h>
#include <atomic>
#include <boost/thread.hpp>
#include <iostream>
#include <vector>
#include "core_tests/util.hpp"
#include "create_samples.hpp"
#include "dag.hpp"
#include "libdevcore/DBFactory.h"
#include "libdevcore/Log.h"
#include "libweb3jsonrpc/RpcServer.h"
#include "libweb3jsonrpc/Taraxa.h"
#include "network.hpp"
#include "pbft_chain.hpp"
#include "pbft_manager.hpp"
#include "sortition.h"
#include "string"
#include "top.hpp"
#include "util.hpp"

namespace taraxa {
using namespace core_tests::util;
using samples::sendTrx;

const unsigned NUM_TRX = 200;
const unsigned SYNC_TIMEOUT = 400;
auto g_secret = dev::Secret(
    "3800b2875669d9b2053c1aff9224ecfdc411423aac5b5a73d7a45ced1c3b9dcd",
    dev::Secret::ConstructFromStringType::FromHex);
auto g_key_pair = dev::KeyPair(g_secret);
auto g_trx_signed_samples =
    samples::createSignedTrxSamples(0, NUM_TRX, g_secret);
auto g_mock_dag0 = samples::createMockDag0();
auto g_test_account =
    samples::createTestAccountTable("core_tests/account_table.txt");
uint64_t g_init_bal = TARAXA_COINS_DECIMAL / 5;

const char *input1[] = {"./build/main",
                        "--conf_taraxa",
                        "./core_tests/conf/conf_taraxa1.json",
                        "-v",
                        "4",
                        "--destroy_db"};
const char *input2[] = {"./build/main2",
                        "--conf_taraxa",
                        "./core_tests/conf/conf_taraxa2.json",
                        "-v",
                        "-2",
                        "--destroy_db"};
const char *input3[] = {"./build/main3",
                        "--conf_taraxa",
                        "./core_tests/conf/conf_taraxa3.json",
                        "-v",
                        "-2",
                        "--destroy_db"};
const char *input4[] = {"./build/main4",
                        "--conf_taraxa",
                        "./core_tests/conf/conf_taraxa4.json",
                        "-v",
                        "-2",
                        "--destroy_db"};
const char *input5[] = {"./build/main5",
                        "--conf_taraxa",
                        "./core_tests/conf/conf_taraxa5.json",
                        "-v",
                        "-2",
                        "--destroy_db"};

void send_2_nodes_trxs() {
  std::string sendtrx1 =
      R"(curl -m 10 -s -d '{"jsonrpc": "2.0", "id": "0", "method": "create_test_coin_transactions",
                                      "params": [{ "secret": "3800b2875669d9b2053c1aff9224ecfdc411423aac5b5a73d7a45ced1c3b9dcd",
                                      "delay": 500, 
                                      "number": 600, 
                                      "nonce": 0, 
                                      "receiver":"973ecb1c08c8eb5a7eaa0d3fd3aab7924f2838b0"}]}' 0.0.0.0:7777)";
  std::string sendtrx2 =
      R"(curl -m 10 -s -d '{"jsonrpc": "2.0", "id": "0", "method": "create_test_coin_transactions",
                                      "params": [{ "secret": "e6af8ca3b4074243f9214e16ac94831f17be38810d09a3edeb56ab55be848a1e",
                                      "delay": 700, 
                                      "number": 400, 
                                      "nonce": 0 , 
                                      "receiver":"4fae949ac2b72960fbe857b56532e2d3c8418d5e"}]}' 0.0.0.0:7778)";
  std::cout << "Sending trxs ..." << std::endl;
  std::thread t1([sendtrx1]() { system(sendtrx1.c_str()); });
  std::thread t2([sendtrx2]() { system(sendtrx2.c_str()); });

  t1.join();
  t2.join();
  std::cout << "All trxs sent..." << std::endl;
}

void init_5_nodes_coin() {
  std::string node1to2 = fmt(
      R"(curl -m 10 -s -d '{"jsonrpc": "2.0", "id": "0", "method": "send_coin_transaction",
                                      "params": [{ 
                                        "secret": "3800b2875669d9b2053c1aff9224ecfdc411423aac5b5a73d7a45ced1c3b9dcd",
                                        "value": %s, 
                                        "gas_price": 1, 
                                        "gas": 100000,
                                        "nonce": 0, 
                                        "receiver":"973ecb1c08c8eb5a7eaa0d3fd3aab7924f2838b0"}]}' 0.0.0.0:7777 > /dev/null)",
      g_init_bal);
  std::string node1to3 = fmt(
      R"(curl -m 10 -s -d '{"jsonrpc": "2.0", "id": "0", "method": "send_coin_transaction",
                                      "params": [{ 
                                        "secret": "3800b2875669d9b2053c1aff9224ecfdc411423aac5b5a73d7a45ced1c3b9dcd",
                                        "value": %s, 
                                        "gas_price": 1, 
                                        "gas": 100000,
                                        "nonce": 1, 
                                        "receiver":"4fae949ac2b72960fbe857b56532e2d3c8418d5e"}]}' 0.0.0.0:7777 > /dev/null)",
      g_init_bal);

  std::string node1to4 = fmt(
      R"(curl -m 10 -s -d '{"jsonrpc": "2.0", "id": "0", "method": "send_coin_transaction",
                                      "params": [{ 
                                        "secret": "3800b2875669d9b2053c1aff9224ecfdc411423aac5b5a73d7a45ced1c3b9dcd",
                                        "value": %s, 
                                        "gas_price": 1, 
                                        "gas": 100000,
                                        "nonce": 2, 
                                        "receiver":"415cf514eb6a5a8bd4d325d4874eae8cf26bcfe0"}]}' 0.0.0.0:7777 > /dev/null)",
      g_init_bal);

  std::string node1to5 = fmt(
      R"(curl -m 10 -s -d '{"jsonrpc": "2.0", "id": "0", "method": "send_coin_transaction",
                                      "params": [{ 
                                        "secret": "3800b2875669d9b2053c1aff9224ecfdc411423aac5b5a73d7a45ced1c3b9dcd",
                                        "value": %s, 
                                        "gas_price": 1, 
                                        "gas": 100000,
                                        "nonce": 3, 
                                        "receiver":"b770f7a99d0b7ad9adf6520be77ca20ee99b0858"}]}' 0.0.0.0:7777 > /dev/null)",
      g_init_bal);
  std::cout << "Init 5 node coins ..." << std::endl;

  std::thread t1([node1to2]() { system(node1to2.data()); });
  std::thread t2([node1to3]() { system(node1to3.data()); });
  std::thread t3([node1to4]() { system(node1to4.data()); });
  std::thread t4([node1to5]() { system(node1to5.data()); });
  t1.join();
  t2.join();
  t3.join();
  t4.join();
}

void send_5_nodes_trxs(bool coin_distributed) {
  std::string sendtrx1;
  if (!coin_distributed) {
    sendtrx1 =
        R"(curl -m 10 -s -d '{"jsonrpc": "2.0", "id": "0", "method": "create_test_coin_transactions",
                                      "params": [{ "secret": "3800b2875669d9b2053c1aff9224ecfdc411423aac5b5a73d7a45ced1c3b9dcd",
                                      "delay": 5, 
                                      "number": 2000, 
                                      "nonce": 0, 
                                      "receiver":"973ecb1c08c8eb5a7eaa0d3fd3aab7924f2838b0" }]}' 0.0.0.0:7777 > /dev/null )";
  } else {
    // nonce start from 4
    sendtrx1 =
        R"(curl -m 10 -s -d '{"jsonrpc": "2.0", "id": "0", "method": "create_test_coin_transactions",
                                      "params": [{ "secret": "3800b2875669d9b2053c1aff9224ecfdc411423aac5b5a73d7a45ced1c3b9dcd",
                                      "delay": 5, 
                                      "number": 2000, 
                                      "nonce": 4, 
                                      "receiver":"973ecb1c08c8eb5a7eaa0d3fd3aab7924f2838b0" }]}' 0.0.0.0:7777 > /dev/null )";
  }
  std::string sendtrx2 =
      R"(curl -m 10 -s -d '{"jsonrpc": "2.0", "id": "0", "method": "create_test_coin_transactions",
                                      "params": [{ "secret": "e6af8ca3b4074243f9214e16ac94831f17be38810d09a3edeb56ab55be848a1e",
                                      "delay": 7, 
                                      "number": 2000, 
                                      "nonce": 0,
                                      "receiver":"4fae949ac2b72960fbe857b56532e2d3c8418d5e" }]}' 0.0.0.0:7778 > /dev/null)";
  std::string sendtrx3 =
      R"(curl -m 10 -s -d '{"jsonrpc": "2.0", "id": "0", "method": "create_test_coin_transactions",
                                      "params": [{ "secret": "f1261c9f09b0b483486c3b298f7c1ee001ff37e10023596528af93e34ba13f5f",
                                      "delay": 3, 
                                      "number": 2000, 
                                      "nonce": 0,
                                      "receiver":"415cf514eb6a5a8bd4d325d4874eae8cf26bcfe0" }]}' 0.0.0.0:7779 > /dev/null)";
  std::string sendtrx4 =
      R"(curl -m 10 -s -d '{"jsonrpc": "2.0", "id": "0", "method": "create_test_coin_transactions",
                                      "params": [{ "secret": "7f38ee36812f2e4b1d75c9d21057fd718b9e7903ee9f9d4eb93b690790bb4029",
                                      "delay": 10, 
                                      "number": 2000, 
                                      "nonce": 0,
                                      "receiver":"b770f7a99d0b7ad9adf6520be77ca20ee99b0858" }]}' 0.0.0.0:7780 > /dev/null)";
  std::string sendtrx5 =
      R"(curl -m 10 -s -d '{"jsonrpc": "2.0", "id": "0", "method": "create_test_coin_transactions",
                                      "params": [{ "secret": "beb2ed10f80e3feaf971614b2674c7de01cfd3127faa1bd055ed50baa1ce34fe",
                                      "delay": 2,
                                      "number": 2000, 
                                      "nonce": 0,
                                      "receiver":"d79b2575d932235d87ea2a08387ae489c31aa2c9" }]}' 0.0.0.0:7781 > /dev/null)";
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
}

void send_dummy_trx(bool coin_distributed) {
  std::cout << "Wait for 40 seconds before sent dummy transaction ..."
            << std::endl;

  taraxa::thisThreadSleepForSeconds(40);
  std::string dummy_trx;
  if (!coin_distributed) {
    dummy_trx =
        R"(curl -m 10 -s -d '{"jsonrpc": "2.0", "id": "0", "method": "send_coin_transaction",
                                      "params": [{ 
                                        "secret": "3800b2875669d9b2053c1aff9224ecfdc411423aac5b5a73d7a45ced1c3b9dcd",
                                        "value": 0, 
                                        "gas_price": 1, 
                                        "gas": 100000,
                                        "nonce": 2000, 
                                        "receiver":"973ecb1c08c8eb5a7eaa0d3fd3aab7924f2838b0"}]}' 0.0.0.0:7777 > /dev/null)";
  } else {
    dummy_trx =
        R"(curl -m 10 -s -d '{"jsonrpc": "2.0", "id": "0", "method": "send_coin_transaction",
                                      "params": [{ 
                                        "secret": "3800b2875669d9b2053c1aff9224ecfdc411423aac5b5a73d7a45ced1c3b9dcd",
                                        "value": 0, 
                                        "gas_price": 1, 
                                        "gas": 100000,
                                        "nonce": 2004, 
                                        "receiver":"973ecb1c08c8eb5a7eaa0d3fd3aab7924f2838b0"}]}' 0.0.0.0:7777 > /dev/null)";
  }
  std::cout << "Send dummy transaction ..." << std::endl;
  system(dummy_trx.c_str());
  taraxa::thisThreadSleepForSeconds(2);
}
struct TopTest : public DBUsingTest<> {};
struct FullNodeTest : public DBUsingTest<> {};

TEST_F(FullNodeTest, construct) {
  boost::asio::io_context context;
  auto node(std::make_shared<taraxa::FullNode>(
      context, std::string("./core_tests/conf/conf_taraxa1.json")));
}

TEST_F(TopTest, DISABLED_top_reset) {
  const char *input1[] = {"./build/main",
                          "--conf_taraxa",
                          "./core_tests/conf/conf_taraxa1.json",
                          "-v",
                          "0",
                          "--destroy_db"};
  const char *input2[] = {"./build/main2",
                          "--conf_taraxa",
                          "./core_tests/conf/conf_taraxa2.json",
                          "-v",
                          "0",
                          "--destroy_db"};
  try {
    std::cout << "Copying main2 ..." << std::endl;
    system("cp ./build/main ./build/main2");
  } catch (std::exception &e) {
    std::cerr << e.what() << std::endl;
  }

  Top top1(6, input1);
  EXPECT_TRUE(top1.isActive());
  std::cout << "Top1 created ..." << std::endl;

  Top top2(6, input2);
  EXPECT_TRUE(top2.isActive());
  std::cout << "Top2 created ..." << std::endl;
  std::cout << "Sleep for 1 second ..." << std::endl;
  taraxa::thisThreadSleepForMilliSeconds(1000);

  try {
    send_2_nodes_trxs();
  } catch (std::exception &e) {
    std::cerr << e.what() << std::endl;
  }

  auto node1 = top1.getNode();
  auto node2 = top2.getNode();

  EXPECT_NE(node1, nullptr);
  EXPECT_NE(node2, nullptr);

  // check dags
  auto num_vertices1 = node1->getNumVerticesInDag();
  auto num_vertices2 = node2->getNumVerticesInDag();

  for (auto i = 0; i < SYNC_TIMEOUT; i++) {
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

  top2.stop();
  top1.stop();

  // ------------------------ Reset -----------------
  top1.reset();
  std::cout << "Top1 reset ...\n";
  top2.reset();
  std::cout << "Top2 reset ...\n";
  top1.start();
  top2.start();
  taraxa::thisThreadSleepForMilliSeconds(2500);
  node1 = top1.getNode();
  node2 = top2.getNode();
  EXPECT_NE(node1, nullptr);
  EXPECT_NE(node2, nullptr);

  try {
    send_2_nodes_trxs();
  } catch (std::exception &e) {
    std::cerr << e.what() << std::endl;
  }
  num_vertices1 = node1->getNumVerticesInDag();
  num_vertices2 = node2->getNumVerticesInDag();

  for (auto i = 0; i < SYNC_TIMEOUT; i++) {
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
  } catch (std::exception &e) {
    std::cerr << e.what() << std::endl;
  }
}

TEST_F(FullNodeTest, DISABLED_full_node_reset) {
  boost::asio::io_context context1;
  boost::asio::io_context context2;

  auto node1(std::make_shared<taraxa::FullNode>(
      context1, std::string("./core_tests/conf/conf_taraxa1.json")));

  node1->setDebug(true);
  node1->start(true);  // boot node

  // send package
  FullNodeConfig conf2("./core_tests/conf/conf_taraxa2.json");
  auto nw2(std::make_shared<taraxa::Network>(
      conf2.network, conf2.genesis_state.block.getHash().toString()));

  std::unique_ptr<boost::asio::io_context::work> work(
      new boost::asio::io_context::work(context1));

  boost::thread t([&context1]() { context1.run(); });

  nw2->start();
  std::vector<DagBlock> blks;

  DagBlock blk1(conf2.genesis_state.block.getHash(), 0, {}, {}, sig_t(77777),
                blk_hash_t(1), addr_t(16));
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
  for (auto i = 0; i < SYNC_TIMEOUT; ++i) {
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

  // // TODO: pbft does not support node stop yet, to be fixed ...
  // node1->getPbftManager()->stop();
  std::cout << "Waiting connection for 100 milliseconds ..." << std::endl;
  for (auto i = 0; i < 100; i++) {
    if (node1->getPeerCount() > 0) break;
    taraxa::thisThreadSleepForMilliSeconds(100);
  }

  for (auto i = 0; i < blks.size(); ++i) {
    nw2->sendBlock(node1->getNetwork()->getNodeId(), blks[i], true);
  }

  taraxa::thisThreadSleepForMilliSeconds(1000);

  num_received_blks = node1->getNumReceivedBlocks();
  num_vertices_in_dag = node1->getNumVerticesInDag().first;
  for (auto i = 0; i < SYNC_TIMEOUT; ++i) {
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

// fixme: flaky
TEST_F(TopTest, sync_five_nodes) {
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
  } catch (std::exception &e) {
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

  ASSERT_GT(node1->getPeerCount(), 0);
  ASSERT_GT(node2->getPeerCount(), 0);
  ASSERT_GT(node3->getPeerCount(), 0);
  ASSERT_GT(node4->getPeerCount(), 0);
  ASSERT_GT(node5->getPeerCount(), 0);
  std::vector<std::shared_ptr<FullNode>> nodes;
  nodes.emplace_back(node1);
  nodes.emplace_back(node2);
  nodes.emplace_back(node3);
  nodes.emplace_back(node4);
  nodes.emplace_back(node5);

  EXPECT_EQ(node1->getDagBlockMaxHeight(), 1);  // genesis block

  // transfer some coins to your friends ...
  init_5_nodes_coin();

  taraxa::thisThreadSleepForSeconds(5);
  for (auto i = 0; i < SYNC_TIMEOUT; i++) {
    if (i % 10 == 0) {
      std::cout << "Wait for init balance syncing ..." << std::endl;
    }

    bool ok = true;
    // for each node, check initial balances
    for (auto const &node : nodes) {
      if (!((node->getBalance(
                     addr_t("de2b1203d72d3549ee2f733b00b2789414c7cea5"))
                 .first == TARAXA_COINS_DECIMAL - g_init_bal * 4) &&
            node->getBalance(addr_t("973ecb1c08c8eb5a7eaa0d3fd3aab7924f2838b0"))
                    .first == g_init_bal &&
            node->getBalance(addr_t("4fae949ac2b72960fbe857b56532e2d3c8418d5e"))
                    .first == g_init_bal &&
            node->getBalance(addr_t("415cf514eb6a5a8bd4d325d4874eae8cf26bcfe0"))
                    .first == g_init_bal &&
            node->getBalance(addr_t("b770f7a99d0b7ad9adf6520be77ca20ee99b0858"))
                    .first == g_init_bal)) {
        ok = false;
      }
    }
    if (ok) {
      break;
    }
    taraxa::thisThreadSleepForMilliSeconds(200);
  }

  // make sure all accounts init balance are finished
  for (auto const &node : nodes) {
    ASSERT_EQ(
        node->getBalance(addr_t("de2b1203d72d3549ee2f733b00b2789414c7cea5"))
            .first,
        TARAXA_COINS_DECIMAL - g_init_bal * 4);
    ASSERT_EQ(
        node->getBalance(addr_t("973ecb1c08c8eb5a7eaa0d3fd3aab7924f2838b0"))
            .first,
        g_init_bal);
    ASSERT_EQ(
        node->getBalance(addr_t("4fae949ac2b72960fbe857b56532e2d3c8418d5e"))
            .first,
        g_init_bal);
    ASSERT_EQ(
        node->getBalance(addr_t("415cf514eb6a5a8bd4d325d4874eae8cf26bcfe0"))
            .first,
        g_init_bal);
    ASSERT_EQ(
        node->getBalance(addr_t("b770f7a99d0b7ad9adf6520be77ca20ee99b0858"))
            .first,
        g_init_bal);
  }
  std::cout << "Balance initialized ... " << std::endl;
  // send 1000 trxs
  try {
    send_5_nodes_trxs(true);
  } catch (std::exception &e) {
    std::cerr << e.what() << std::endl;
  }

  // check dags
  auto num_vertices1 = node1->getNumVerticesInDag();
  auto num_vertices2 = node2->getNumVerticesInDag();
  auto num_vertices3 = node3->getNumVerticesInDag();
  auto num_vertices4 = node4->getNumVerticesInDag();
  auto num_vertices5 = node5->getNumVerticesInDag();

  for (auto i = 0; i < SYNC_TIMEOUT; i++) {
    if (i % 10 == 0) {
      std::cout << "Wait for vertices syncing ..." << std::endl;
      std::cout << " Node 1: Dag size = " << node1->getNumVerticesInDag().first
                << std::endl
                << " Node 2: Dag size = " << node2->getNumVerticesInDag().first
                << std::endl
                << " Node 3: Dag size = " << node3->getNumVerticesInDag().first
                << std::endl
                << " Node 4: Dag size = " << node4->getNumVerticesInDag().first
                << std::endl
                << " Node 5: Dag size = " << node5->getNumVerticesInDag().first
                << std::endl;
    }
    num_vertices1 = node1->getNumVerticesInDag();
    num_vertices2 = node2->getNumVerticesInDag();
    num_vertices3 = node3->getNumVerticesInDag();
    num_vertices4 = node4->getNumVerticesInDag();
    num_vertices5 = node5->getNumVerticesInDag();

    if (num_vertices1 == num_vertices2 && num_vertices2 == num_vertices3 &&
        num_vertices3 == num_vertices4 && num_vertices4 == num_vertices5 &&
        node1->getTransactionStatusCount() == 10004 &&
        node2->getTransactionStatusCount() == 10004 &&
        node3->getTransactionStatusCount() == 10004 &&
        node4->getTransactionStatusCount() == 10004 &&
        node5->getTransactionStatusCount() == 10004)
      break;
    taraxa::thisThreadSleepForMilliSeconds(500);
  }

  EXPECT_GT(node1->getNumProposedBlocks(), 2);
  EXPECT_GT(node2->getNumProposedBlocks(), 2);
  EXPECT_GT(node3->getNumProposedBlocks(), 2);
  EXPECT_GT(node4->getNumProposedBlocks(), 2);
  EXPECT_GT(node5->getNumProposedBlocks(), 2);

  ASSERT_EQ(node1->getTransactionStatusCount(), 10004);
  ASSERT_EQ(node2->getTransactionStatusCount(), 10004);
  ASSERT_EQ(node3->getTransactionStatusCount(), 10004);
  ASSERT_EQ(node4->getTransactionStatusCount(), 10004);
  ASSERT_EQ(node5->getTransactionStatusCount(), 10004);
  // send dummy trx to make sure all DAGs are ordered
  try {
    send_dummy_trx(true);
  } catch (std::exception &e) {
    std::cerr << e.what() << std::endl;
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

  ASSERT_EQ(node1->getTransactionStatusCount(), 10005);
  ASSERT_EQ(node2->getTransactionStatusCount(), 10005);
  ASSERT_EQ(node3->getTransactionStatusCount(), 10005);
  ASSERT_EQ(node4->getTransactionStatusCount(), 10005);
  ASSERT_EQ(node5->getTransactionStatusCount(), 10005);

  std::cout << "All transactions received ..." << std::endl;
  // wait for trx execution ...
  taraxa::thisThreadSleepForSeconds(10);
  uint64_t trx_executed1, trx_executed2, trx_executed3, trx_executed4,
      trx_executed5;

  auto TIMEOUT = SYNC_TIMEOUT * 10;
  for (auto i = 0; i < TIMEOUT; i++) {
    trx_executed1 = node1->getNumTransactionExecuted();
    trx_executed2 = node2->getNumTransactionExecuted();
    trx_executed3 = node3->getNumTransactionExecuted();
    trx_executed4 = node4->getNumTransactionExecuted();
    trx_executed5 = node5->getNumTransactionExecuted();

    auto trx_packed1 = node1->getPackedTrxs().size();
    auto trx_packed2 = node2->getPackedTrxs().size();
    auto trx_packed3 = node3->getPackedTrxs().size();
    auto trx_packed4 = node4->getPackedTrxs().size();
    auto trx_packed5 = node5->getPackedTrxs().size();

    if (trx_packed1 < trx_executed1) {
      std::cout << "Warning! " << trx_packed1
                << " packed transactions is less than " << trx_executed1
                << " executed transactions in node 1";
    }

    if (trx_executed1 == 10005 && trx_executed2 == 10005 &&
        trx_executed3 == 10005 && trx_executed4 == 10005 &&
        trx_executed5 == 10005) {
      if (trx_packed1 == 10005 && trx_packed2 == 10005 &&
          trx_packed3 == 10005 && trx_packed4 == 10005 &&
          trx_packed5 == 10005) {
        break;
      } else {
        std::cout << "Warning! See all nodes with 10005 executed transactions, "
                     "but not all nodes yet see 10005 packed transactions!!!";
      }
    }
    taraxa::thisThreadSleepForMilliSeconds(500);
    if (i % 100 == 0) {
      if (trx_executed1 != 10005) {
        std::cout << " Node 1: executed blk= " << node2->getNumBlockExecuted()
                  << " Dag size: " << node1->getNumVerticesInDag().first
                  << " executed trx = " << trx_executed2 << "/10005"
                  << std::endl;
      }
      if (trx_executed2 != 10005) {
        std::cout << " Node 2: executed blk= " << node2->getNumBlockExecuted()
                  << " Dag size: " << node2->getNumVerticesInDag().first
                  << " executed trx = " << trx_executed2 << "/10005"
                  << std::endl;
      }
      if (trx_executed3 != 10005) {
        std::cout << " Node 3: executed blk= " << node3->getNumBlockExecuted()
                  << " Dag size: " << node3->getNumVerticesInDag().first
                  << " executed trx = " << trx_executed3 << "/10005"
                  << std::endl;
      }
      if (trx_executed4 != 10005) {
        std::cout << " Node 4: executed blk= " << node4->getNumBlockExecuted()
                  << " Dag size: " << node4->getNumVerticesInDag().first
                  << " executed trx = " << trx_executed4 << "/10005"
                  << std::endl;
      }
      if (trx_executed5 != 10005) {
        std::cout << " Node 5: executed blk= " << node5->getNumBlockExecuted()
                  << " Dag size: " << node5->getNumVerticesInDag().first
                  << " executed trx = " << trx_executed5 << "/10005"
                  << std::endl;
      }
    }
    if (i == TIMEOUT - 1) {
      // last wait
      std::cout << "Warning! Syncing maybe failed ..." << std::endl;
    }
  }

  auto k = 0;
  for (auto const &node : nodes) {
    k++;
    auto vertices_diff =
        node->getNumVerticesInDag().first - 1 - node->getNumBlockExecuted();
    if (vertices_diff >= 5 || vertices_diff < 0 ||
        node->getNumTransactionExecuted() != 10005 ||
        node->getPackedTrxs().size() != 10005) {
      std::cout << "Node " << k
                << " :Number of trx packed = " << node->getPackedTrxs().size()
                << std::endl;
      std::cout << "Node " << k << " :Number of trx executed = "
                << node->getNumTransactionExecuted() << std::endl;

      std::cout << "Node " << k << " :Number of block executed = "
                << node->getNumBlockExecuted() << std::endl;
      auto num_vertices = node->getNumVerticesInDag();
      std::cout << "Node " << k
                << " :Number of vertices in Dag = " << num_vertices.first
                << " , " << num_vertices.second << std::endl;
      auto dags = node->getLinearizedDagBlocks();
      for (auto i(0); i < dags.size(); ++i) {
        auto d = dags[i];
        std::cout << i << " " << d
                  << " trx: " << node1->getDagBlock(d)->getTrxs().size()
                  << std::endl;
      }
      std::string filename = "debug_dag_" + std::to_string(k);
      node->drawGraph(filename);
    }
  }

  k = 0;
  for (auto const &node : nodes) {
    k++;

    EXPECT_EQ(node->getNumTransactionExecuted(), 10005)
        << " \nNum executed in node " << k << " node " << node
        << " is : " << node->getNumTransactionExecuted()
        << " \nNum linearized blks: " << node->getLinearizedDagBlocks().size()
        << " \nNum executed blks: " << node->getNumBlockExecuted()
        << " \nNum vertices in DAG: " << node->getNumVerticesInDag().first
        << " " << node->getNumVerticesInDag().second << "\n";

    auto vertices_diff =
        node->getNumVerticesInDag().first - 1 - node->getNumBlockExecuted();

    // diff should be larger than 0 but smaller than number of nodes
    // genesis block won't be executed
    EXPECT_LT(vertices_diff, 5)
        << "Failed in node " << k << "node " << node
        << " Number of vertices: " << node->getNumVerticesInDag().first
        << " Number of executed blks: " << node->getNumBlockExecuted()
        << std::endl;
    EXPECT_GE(vertices_diff, 0)
        << "Failed in node " << k << "node " << node
        << " Number of vertices: " << node->getNumVerticesInDag().first
        << " Number of executed blks: " << node->getNumBlockExecuted()
        << std::endl;
    EXPECT_EQ(node->getPackedTrxs().size(), 10005);
  }

  auto dags = node1->getLinearizedDagBlocks();
  for (auto i(0); i < dags.size(); ++i) {
    auto d = dags[i];
    for (auto const &t : node1->getDagBlock(d)->getTrxs()) {
      auto blk = node1->getDagBlockFromTransaction(t);
      EXPECT_FALSE(blk.isZero());
    }
  }

  for (auto const &node : nodes) {
    EXPECT_EQ(
        node->getBalance(addr_t("de2b1203d72d3549ee2f733b00b2789414c7cea5"))
            .first,
        TARAXA_COINS_DECIMAL - g_init_bal * 4 - 2000 * 100);
    EXPECT_EQ(
        node->getBalance(addr_t("973ecb1c08c8eb5a7eaa0d3fd3aab7924f2838b0"))
            .first,
        g_init_bal);
    EXPECT_EQ(
        node->getBalance(addr_t("4fae949ac2b72960fbe857b56532e2d3c8418d5e"))
            .first,
        g_init_bal);
    EXPECT_EQ(
        node->getBalance(addr_t("415cf514eb6a5a8bd4d325d4874eae8cf26bcfe0"))
            .first,
        g_init_bal);
    EXPECT_EQ(
        node->getBalance(addr_t("b770f7a99d0b7ad9adf6520be77ca20ee99b0858"))
            .first,
        g_init_bal);
    EXPECT_EQ(
        node->getBalance(addr_t("d79b2575d932235d87ea2a08387ae489c31aa2c9"))
            .first,
        2000 * 100);
  }
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
  } catch (std::exception &e) {
    std::cerr << e.what() << std::endl;
  }
}

TEST_F(FullNodeTest, insert_anchor_and_compute_order) {
  boost::asio::io_context context;
  auto node(std::make_shared<taraxa::FullNode>(
      context, std::string("./core_tests/conf/conf_taraxa1.json")));
  node->start(true);  // boot node

  g_mock_dag0 = samples::createMockDag0(
      node->getConfig().genesis_state.block.getHash().toString());

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
  auto num_blks_set = node->setDagBlockOrder(blk_hash_t(pivot), period);
  EXPECT_EQ(num_blks_set, 6);
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
  num_blks_set = node->setDagBlockOrder(blk_hash_t(pivot), period);
  EXPECT_EQ(num_blks_set, 7);

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
  num_blks_set = node->setDagBlockOrder(blk_hash_t(pivot), period);
  EXPECT_EQ(num_blks_set, 5);

  node->stop();
}

TEST_F(TopTest, create_top_level_db) {
  {
    dev::db::setDatabaseKind(dev::db::DatabaseKind::LevelDB);

    const char *inputs[] = {"./build/main", "--conf_taraxa",
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

TEST_F(TopTest, create_top_memory_db) {
  {
    const char *inputs[] = {"./build/main", "--conf_taraxa",
                            "./core_tests/conf/conf_taraxa1.json", "-v", "0"};
    Top top(5, input1);
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

TEST(Top, destroy_db) {
  dev::db::setDatabaseKind(dev::db::DatabaseKind::LevelDB);

  {
    boost::asio::io_context context;
    FullNodeConfig conf("./core_tests/conf/conf_taraxa1.json");
    auto node(std::make_shared<taraxa::FullNode>(context, conf,
                                                 true));  // destroy DB
    node->start(false);
    auto trx_db = node->getTrxsDB();
    trx_db->put(g_trx_signed_samples[0].getHash().toString(),
                g_trx_signed_samples[0].getJsonStr());
    // Verify trx saved in db
    EXPECT_TRUE(
        !trx_db->get(g_trx_signed_samples[0].getHash().toString()).empty());
    trx_db->commit();
    trx_db = nullptr;
    node->stop();
  }

  {
    boost::asio::io_context context;
    FullNodeConfig conf("./core_tests/conf/conf_taraxa1.json");
    auto node(std::make_shared<taraxa::FullNode>(context, conf,
                                                 false));  // destroy DB
    node->start(false);
    auto trx_db = node->getTrxsDB();
    // Verify trx saved in db after restart with destroy_db false
    EXPECT_TRUE(
        !trx_db->get(g_trx_signed_samples[0].getHash().toString()).empty());
    trx_db = nullptr;
    node->stop();
  }

  {
    boost::asio::io_context context;
    FullNodeConfig conf("./core_tests/conf/conf_taraxa1.json");
    auto node(std::make_shared<taraxa::FullNode>(context, conf,
                                                 true));  // destroy DB
    node->start(false);
    auto trx_db = node->getTrxsDB();
    // Verify trx not in db after restart with destroy_db true
    EXPECT_TRUE(
        trx_db->get(g_trx_signed_samples[0].getHash().toString()).empty());
    trx_db = nullptr;
    node->stop();
  }

  dev::db::setDatabaseKind(dev::db::DatabaseKind::MemoryDB);
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
    auto node(std::make_shared<taraxa::FullNode>(context, conf,
                                                 true));  // destroy DB

    g_mock_dag0 =
        samples::createMockDag0(conf.genesis_state.block.getHash().toString());

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
    std::cout << "Reset Node ..." << std::endl;
    node->reset();
    std::cout << "Node reset ..." << std::endl;

    node->start(false);

    // TODO: pbft does not support node stop yet, to be fixed ...
    node->getPbftManager()->stop();
    for (int i = 1; i < num_blks; i++) {
      node->insertBlock(g_mock_dag0[i]);
    }
    taraxa::thisThreadSleepForMilliSeconds(1000);

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

TEST_F(TopTest, sync_two_nodes1) {
  Top top1(6, input1);
  EXPECT_TRUE(top1.isActive());
  std::cout << "Top1 created ..." << std::endl;

  thisThreadSleepForMilliSeconds(500);

  // copy main2
  try {
    std::cout << "Copying main2 ..." << std::endl;
    system("cp ./build/main ./build/main2");
  } catch (std::exception &e) {
    std::cerr << e.what() << std::endl;
  }
  top1.stop();
  top1.start(6, input1);
  taraxa::thisThreadSleepForMilliSeconds(500);

  Top top2(6, input2);
  EXPECT_TRUE(top2.isActive());
  std::cout << "Top2 created ..." << std::endl;
  // wait for top2 initialize
  taraxa::thisThreadSleepForMilliSeconds(1000);

  auto node1 = top1.getNode();
  auto node2 = top2.getNode();
  EXPECT_NE(node1, nullptr);
  EXPECT_NE(node2, nullptr);

  EXPECT_GT(node1->getPeerCount(), 0);
  EXPECT_GT(node2->getPeerCount(), 0);

  // send 1000 trxs
  try {
    send_2_nodes_trxs();
  } catch (std::exception &e) {
    std::cerr << e.what() << std::endl;
  }

  auto num_trx1 = node1->getTransactionStatusCount();
  auto num_trx2 = node2->getTransactionStatusCount();
  // add more delay if sync is not done
  for (auto i = 0; i < SYNC_TIMEOUT; i++) {
    if (num_trx1 == 1000 && num_trx2 == 1000) break;
    taraxa::thisThreadSleepForMilliSeconds(500);
    num_trx1 = node1->getTransactionStatusCount();
    num_trx2 = node2->getTransactionStatusCount();
  }
  EXPECT_EQ(node1->getTransactionStatusCount(), 1000);
  EXPECT_EQ(node2->getTransactionStatusCount(), 1000);

  auto num_vertices1 = node1->getNumVerticesInDag();
  auto num_vertices2 = node2->getNumVerticesInDag();
  for (auto i = 0; i < SYNC_TIMEOUT; i++) {
    if (num_vertices1.first > 3 && num_vertices2.first > 3 &&
        num_vertices1 == num_vertices2)
      break;
    taraxa::thisThreadSleepForMilliSeconds(500);
    num_vertices1 = node1->getNumVerticesInDag();
    num_vertices2 = node2->getNumVerticesInDag();
  }
  EXPECT_GE(num_vertices1.first, 3);
  EXPECT_GE(num_vertices2.second, 3);
  EXPECT_EQ(num_vertices1, num_vertices2);

  top2.kill();
  top1.kill();
  // delete main2
  try {
    std::cout << "main2 deleted ..." << std::endl;
    system("rm -f ./build/main2");
  } catch (std::exception &e) {
    std::cerr << e.what() << std::endl;
  }
}

TEST_F(TopTest, sync_two_nodes2) {
  Top top1(6, input1);
  EXPECT_TRUE(top1.isActive());
  std::cout << "Top1 created ..." << std::endl;

  thisThreadSleepForMilliSeconds(500);

  // send 1000 trxs
  try {
    std::cout << "Sending 1000 trxs ..." << std::endl;
    sendTrx(1000, 7777);
    std::cout << "1000 trxs sent ..." << std::endl;

  } catch (std::exception &e) {
    std::cerr << e.what() << std::endl;
  }

  top1.stop();
  top1.start(6, input1);
  taraxa::thisThreadSleepForMilliSeconds(500);

  // copy main2
  try {
    std::cout << "Copying main2 ..." << std::endl;
    system("cp ./build/main ./build/main2");
  } catch (std::exception &e) {
    std::cerr << e.what() << std::endl;
  }

  Top top2(6, input2);
  EXPECT_TRUE(top2.isActive());
  std::cout << "Top2 created ..." << std::endl;
  // wait for top2 initialize
  taraxa::thisThreadSleepForMilliSeconds(1000);
  auto node1 = top1.getNode();
  auto node2 = top2.getNode();
  EXPECT_NE(node1, nullptr);
  EXPECT_NE(node2, nullptr);

  EXPECT_GT(node1->getPeerCount(), 0);
  EXPECT_GT(node2->getPeerCount(), 0);

  auto vertices1 = node1->getNumVerticesInDag();
  auto vertices2 = node2->getNumVerticesInDag();
  // let node2 sync node1
  for (auto i = 0; i < SYNC_TIMEOUT; i++) {
    if (vertices1 == vertices2 && vertices1.first > 3) break;
    taraxa::thisThreadSleepForMilliSeconds(500);
    vertices1 = node1->getNumVerticesInDag();
    vertices2 = node2->getNumVerticesInDag();
  }
  EXPECT_GE(vertices1.first, 3);
  EXPECT_GE(vertices1.second, 3);
  EXPECT_EQ(vertices1, vertices2);
  top2.kill();
  top1.kill();
  // delete main2
  try {
    std::cout << "main2 deleted ..." << std::endl;
    system("rm -f ./build/main2");
  } catch (std::exception &e) {
    std::cerr << e.what() << std::endl;
  }
}

TEST_F(TopTest, DISABLED_sync_two_nodes3) {
  Top top1(6, input1);
  EXPECT_TRUE(top1.isActive());
  std::cout << "Top1 created ..." << std::endl;

  thisThreadSleepForMilliSeconds(500);

  // copy main2
  try {
    std::cout << "Copying main2 ..." << std::endl;
    system("cp ./build/main ./build/main2");
  } catch (std::exception &e) {
    std::cerr << e.what() << std::endl;
  }
  top1.stop();
  top1.start(6, input1);
  auto node1 = top1.getNode();
  taraxa::thisThreadSleepForMilliSeconds(500);
  {
    Top top2(6, input2);
    EXPECT_TRUE(top2.isActive());
    std::cout << "Top2 created ..." << std::endl;
    // wait for top2 initialize
    taraxa::thisThreadSleepForMilliSeconds(1000);

    auto node2 = top2.getNode();
    EXPECT_NE(node1, nullptr);
    EXPECT_NE(node2, nullptr);

    EXPECT_GT(node1->getPeerCount(), 0);
    EXPECT_GT(node2->getPeerCount(), 0);

    // send 1000 trxs to node 2
    try {
      std::cout << "Sending 1000 trxs ..." << std::endl;
      sendTrx(1000, 7778);
      std::cout << "1000 trxs sent ..." << std::endl;

    } catch (std::exception &e) {
      std::cerr << e.what() << std::endl;
    }

    auto vertices1 = node1->getNumVerticesInDag();
    auto vertices2 = node2->getNumVerticesInDag();
    // add more delay if sync is not done
    for (auto i = 0; i < SYNC_TIMEOUT; i++) {
      if (vertices1 == vertices2 && vertices1.first > 3) break;
      taraxa::thisThreadSleepForMilliSeconds(500);
      vertices1 = node1->getNumVerticesInDag();
      vertices2 = node2->getNumVerticesInDag();
    }
    EXPECT_GE(vertices1.first, 3);
    EXPECT_GE(vertices1.second, 3);
    EXPECT_EQ(vertices1, vertices2);
    top2.kill();
  }

  {
    Top top2(6, input2);
    EXPECT_TRUE(top2.isActive());
    std::cout << "Top2 re-created ..." << std::endl;
    // wait for top2 initialize
    taraxa::thisThreadSleepForMilliSeconds(1000);

    auto node2 = top2.getNode();
    EXPECT_NE(node2, nullptr);

    EXPECT_GT(node2->getPeerCount(), 0);
    auto vertices1 = node1->getNumVerticesInDag();
    auto vertices2 = node2->getNumVerticesInDag();
    // add more delay if sync is not done
    for (auto i = 0; i < SYNC_TIMEOUT; i++) {
      if (vertices1 == vertices2 && vertices1.first > 3) break;
      taraxa::thisThreadSleepForMilliSeconds(500);
      vertices1 = node1->getNumVerticesInDag();
      vertices2 = node2->getNumVerticesInDag();
    }
    EXPECT_EQ(vertices1, vertices2);
    top2.kill();
  }

  top1.kill();
  // delete main2
  try {
    std::cout << "main2 deleted ..." << std::endl;
    system("rm -f ./build/main2");
  } catch (std::exception &e) {
    std::cerr << e.what() << std::endl;
  }
}

TEST_F(FullNodeTest, genesis_balance) {
  addr_t addr1(100);
  val_t bal1(1000);
  addr_t addr2(200);
  FullNodeConfig cfg("./core_tests/conf/conf_taraxa1.json");
  cfg.genesis_state.accounts[addr1] = {bal1};
  boost::asio::io_context context;
  auto node(std::make_shared<taraxa::FullNode>(context, cfg));
  auto res = node->getBalance(addr1);
  EXPECT_TRUE(res.second);
  EXPECT_EQ(res.first, bal1);
  res = node->getBalance(addr2);
  EXPECT_FALSE(res.second);
}

// disabled for now, need to create two trx with nonce 0!
TEST_F(TopTest, single_node_run_two_transactions) {
  Top top1(6, input1);
  EXPECT_TRUE(top1.isActive());
  thisThreadSleepForMilliSeconds(500);
  std::cout << "Top1 created ..." << std::endl;
  auto node1 = top1.getNode();
  EXPECT_NE(node1, nullptr);

  std::string send_raw_trx1 =
      R"(curl -m 10 -s -d '{"jsonrpc": "2.0", "id": "0", "method":
"taraxa_sendRawTransaction", "params":
["0xf868808502540be40082520894cb36e7dc45bdf421f6b6f64a75a3760393d3cf598401312d00801ba07659e8c7207a4b2cd96488108fed54c463b1719b438add1159beed04f6660da8a028feb0a3b44bd34e0dd608f82aeeb2cd70d1305653b5dc33678be2ffcfcac997"
                                      ]}' 0.0.0.0:7777)";

  std::string send_raw_trx2 =
      R"(curl -m 10 -s -d '{"jsonrpc": "2.0", "id": "0", "method":
"taraxa_sendRawTransaction", "params":
["0xf868018502540be40082520894cb36e7dc45bdf421f6b6f64a75a3760393d3cf598401312d00801ba05256492dd60623ab5a403ed1b508f845f87f631d2c2e7acd4357cd83ef5b6540a042def7cd4f3c25ce67ee25911740dab47161e096f1dd024badecec58888a890b"
                                      ]}' 0.0.0.0:7777)";

  std::cout << "Send first trx ..." << std::endl;
  system(send_raw_trx1.c_str());
  thisThreadSleepForSeconds(3);
  EXPECT_EQ(node1->getTransactionStatusCount(), 1);
  EXPECT_EQ(node1->getNumVerticesInDag().first, 2);
  std::cout << "First trx received ..." << std::endl;

  auto trx_executed1 = node1->getNumTransactionExecuted();

  for (auto i(0); i < SYNC_TIMEOUT; ++i) {
    trx_executed1 = node1->getNumTransactionExecuted();
    if (trx_executed1 == 1) break;
    thisThreadSleepForMilliSeconds(100);
  }
  EXPECT_EQ(trx_executed1, 1);
  std::cout << "First trx executed ..." << std::endl;
  std::cout << "Send second trx ..." << std::endl;
  system(send_raw_trx2.c_str());
  thisThreadSleepForSeconds(3);
  EXPECT_EQ(node1->getTransactionStatusCount(), 2);
  EXPECT_EQ(node1->getNumVerticesInDag().first, 3);

  trx_executed1 = node1->getNumTransactionExecuted();

  for (auto i(0); i < SYNC_TIMEOUT; ++i) {
    trx_executed1 = node1->getNumTransactionExecuted();
    if (trx_executed1 == 2) break;
    thisThreadSleepForMilliSeconds(100);
  }
  EXPECT_EQ(trx_executed1, 2);
}
// disabled for now, need to create two trx with nonce 0!
TEST_F(TopTest, two_nodes_run_two_transactions) {
  boost::asio::io_context context1;
  FullNodeConfig conf1("./core_tests/conf/conf_taraxa1.json");
  conf1.node_secret =
      "3800b2875669d9b2053c1aff9224ecfdc411423aac5b5a73d7a45ced1c3b9dcd";
  auto node1(std::make_shared<taraxa::FullNode>(context1, conf1));
  boost::asio::io_context context2;
  FullNodeConfig conf2("./core_tests/conf/conf_taraxa2.json");
  conf2.node_secret =
      "e6af8ca3b4074243f9214e16ac94831f17be38810d09a3edeb56ab55be848a1e";
  auto node2(std::make_shared<taraxa::FullNode>(context2, conf2));
  node1->start(true);
  node2->start(false);
  auto rpc = std::make_shared<ModularServer<dev::rpc::TaraxaFace>>(
      new dev::rpc::Taraxa(node1));
  auto rpc_server(
      std::make_shared<taraxa::RpcServer>(context1, conf1.rpc, node1));
  rpc->addConnector(rpc_server);
  rpc_server->StartListening();
  boost::thread t([&context1]() { context1.run(); });
  thisThreadSleepForSeconds(1);

  std::string send_raw_trx1 =
      R"(curl -m 10 -s -d '{"jsonrpc": "2.0", "id": "0", "method":
"taraxa_sendRawTransaction", "params":
["0xf86b808502540be40082520894973ecb1c08c8eb5a7eaa0d3fd3aab7924f2838b087038d7ea4c68000801ca04cef8610e05b4476673c369204899da747a9b32b0ad3769814b620281c900408a0130499a83d0b56c184c6ac518f6bbe2f8f946b65bf42b08cfc9b4dfbe2ebfd04"
                                      ]}' 0.0.0.0:7777)";

  std::string send_raw_trx2 =
      R"(curl -m 10 -s -d '{"jsonrpc": "2.0", "id": "0", "method":
"taraxa_sendRawTransaction", "params":
["0xf86b018502540be40082520894973ecb1c08c8eb5a7eaa0d3fd3aab7924f2838b087038d7ea4c68000801ca06644c30a23286d0de8fa107f1bded3a7a214004042b2007b2a9a62c8b313cf79a06cbb522856838b107542d8213286928500b2822584d6c7c6fee3a2c348cade4a"
                                      ]}' 0.0.0.0:7777)";

  std::cout << "Send first trx ..." << std::endl;
  system(send_raw_trx1.c_str());
  thisThreadSleepForSeconds(3);
  EXPECT_EQ(node1->getTransactionStatusCount(), 1);
  EXPECT_GE(node1->getNumVerticesInDag().first, 2);
  std::cout << "First trx received ..." << std::endl;

  auto trx_executed1 = node1->getNumTransactionExecuted();

  for (auto i(0); i < SYNC_TIMEOUT; ++i) {
    trx_executed1 = node1->getNumTransactionExecuted();
    if (trx_executed1 == 1) break;
    thisThreadSleepForMilliSeconds(100);
  }
  EXPECT_EQ(trx_executed1, 1);
  std::cout << "First trx executed ..." << std::endl;
  std::cout << "Send second trx ..." << std::endl;
  system(send_raw_trx2.c_str());
  thisThreadSleepForSeconds(3);
  EXPECT_EQ(node1->getTransactionStatusCount(), 2);
  EXPECT_GE(node1->getNumVerticesInDag().first, 3);

  trx_executed1 = node1->getNumTransactionExecuted();

  for (auto i(0); i < SYNC_TIMEOUT; ++i) {
    trx_executed1 = node1->getNumTransactionExecuted();
    if (trx_executed1 == 2) break;
    thisThreadSleepForMilliSeconds(1000);
  }
  EXPECT_EQ(trx_executed1, 2);
  rpc_server->StopListening();
  node1->stop();
  node2->stop();
}

TEST_F(FullNodeTest, DISABLED_execute_chain_pbft_transactions) {
  val_t initbal(100000000);  // disable pbft sortition
  std::vector<Transaction> transactions;
  for (auto i = 0; i < NUM_TRX; ++i) {
    transactions.emplace_back(samples::TX_GEN.getWithRandomUniqueSender(
        i * 100, addr_t((i + 1) * 100)));
  }
  FullNodeConfig cfg("./core_tests/conf/conf_taraxa1.json");
  addr_t acc1 = addr(cfg.node_secret);
  cfg.genesis_state.accounts[acc1] = {initbal};
  for (auto const &t : transactions) {
    cfg.genesis_state.accounts[t.getSender()] = {t.getValue()};
  }
  boost::asio::io_context context;
  auto node(std::make_shared<taraxa::FullNode>(context, cfg));
  node->start(true);  // boot node
  auto res = node->getBalance(acc1);
  EXPECT_TRUE(res.second);
  EXPECT_EQ(res.first, initbal);

  for (auto const &t : transactions) {
    node->insertTransaction(t);
    taraxa::thisThreadSleepForMilliSeconds(50);
  }

  taraxa::thisThreadSleepForMilliSeconds(3000);

  EXPECT_GT(node->getNumProposedBlocks(), 0);

  // The test will form a single chain
  std::vector<std::string> ghost;
  node->getGhostPath(cfg.genesis_state.block.getHash().toString(), ghost);
  vec_blk_t blks;
  std::vector<std::vector<uint>> modes;
  ASSERT_GT(ghost.size(), 1);
  uint64_t period = 0, cur_period, cur_period2;
  std::shared_ptr<vec_blk_t> order;
  std::shared_ptr<PbftManager> pbft_mgr = node->getPbftManager();
  // create a period for every 2 pivots, skip genesis
  for (int i = 1; i < ghost.size(); i += 2) {
    auto anchor = blk_hash_t(ghost[i]);
    std::tie(cur_period, order) = node->getDagBlockOrder(anchor);
    // call twice should not change states
    std::tie(cur_period2, order) = node->getDagBlockOrder(anchor);
    EXPECT_EQ(cur_period, cur_period2);
    EXPECT_EQ(cur_period, ++period);
    std::shared_ptr<std::vector<std::pair<blk_hash_t, std::vector<bool>>>>
        trx_overlap_table = node->computeTransactionOverlapTable(order);
    EXPECT_NE(trx_overlap_table, nullptr);
    std::vector<std::vector<uint>> blocks_trx_modes =
        node->createMockTrxSchedule(trx_overlap_table);
    TrxSchedule sche(*order, blocks_trx_modes);
    ScheduleBlock sche_blk(blk_hash_t(100), sche);
    // set period
    node->setDagBlockOrder(anchor, cur_period);
    bool ret = node->executeScheduleBlock(
        sche_blk, pbft_mgr->sortition_account_balance_table, cur_period);
    EXPECT_TRUE(ret);
    taraxa::thisThreadSleepForMilliSeconds(200);
  }
  // pickup the last period when dag (chain) size is odd number
  if (ghost.size() % 2 == 1) {
    std::tie(cur_period, order) =
        node->getDagBlockOrder(blk_hash_t(ghost.back()));
    EXPECT_EQ(cur_period, ++period);
    std::shared_ptr<std::vector<std::pair<blk_hash_t, std::vector<bool>>>>
        trx_overlap_table = node->computeTransactionOverlapTable(order);
    EXPECT_NE(trx_overlap_table, nullptr);
    std::vector<std::vector<uint>> blocks_trx_modes =
        node->createMockTrxSchedule(trx_overlap_table);
    TrxSchedule sche(*order, blocks_trx_modes);
    ScheduleBlock sche_blk(blk_hash_t(100), sche);
    bool ret = node->executeScheduleBlock(
        sche_blk, pbft_mgr->sortition_account_balance_table, cur_period);
    EXPECT_TRUE(ret);
    taraxa::thisThreadSleepForMilliSeconds(200);
  }

  node->stop();
  auto coin_distributed = 0;
  auto i = 0;
  for (auto const &t : g_trx_signed_samples) {
    auto rec = t.getReceiver();
    auto res = node->getBalance(t.getReceiver());
    EXPECT_TRUE(res.second)
        << "Transaction (" << i << ") failed, receiver =" << rec << std::endl;
    EXPECT_EQ(res.first, t.getValue());
    coin_distributed += res.first;
    ++i;
  }
  // TODO because of the nonce rule, testing distributing coins
  // from single account requires more thought
  //  EXPECT_EQ(state->balance(acc1), initbal - coin_distributed);
}

TEST_F(FullNodeTest, DISABLED_send_and_receive_out_order_messages) {
  boost::asio::io_context context1;
  boost::asio::io_context context2;

  auto node1(std::make_shared<taraxa::FullNode>(
      context1, std::string("./core_tests/conf/conf_taraxa1.json")));

  // node1->setVerbose(true);
  node1->setDebug(true);
  node1->start(true);               // boot node
  node1->getPbftManager()->stop();  // boot node

  // send package
  FullNodeConfig conf2("./core_tests/conf/conf_taraxa2.json");
  auto nw2(std::make_shared<taraxa::Network>(
      conf2.network, conf2.genesis_state.block.getHash().toString()));

  std::unique_ptr<boost::asio::io_context::work> work(
      new boost::asio::io_context::work(context1));

  boost::thread t([&context1]() { context1.run(); });

  nw2->start();
  std::vector<DagBlock> blks;

  DagBlock blk1(conf2.genesis_state.block.getHash(), 1, {}, {}, sig_t(77777),
                blk_hash_t(1), addr_t(16));
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

TEST_F(FullNodeTest, save_network_to_file) {
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

    for (int i = 0; i < SYNC_TIMEOUT; i++) {
      taraxa::thisThreadSleepForSeconds(1);
      if (1 == node2->getPeerCount() && 1 == node3->getPeerCount()) break;
    }

    ASSERT_EQ(1, node2->getPeerCount());
    ASSERT_EQ(1, node3->getPeerCount());
    node2->stop();
    node3->stop();
  }
}

TEST_F(FullNodeTest, receive_send_transaction) {
  boost::asio::io_context context1;

  Top top1(5, input1);
  EXPECT_TRUE(top1.isActive());

  auto node1 = top1.getNode();
  node1->setDebug(true);
  node1->start(true);  // boot node

  std::unique_ptr<boost::asio::io_context::work> work(
      new boost::asio::io_context::work(context1));

  try {
    sendTrx(1000, 7777);
  } catch (std::exception &e) {
    std::cerr << e.what() << std::endl;
  }
  std::cout << "1000 transaction are sent through RPC ..." << std::endl;

  auto num_proposed_blk = node1->getNumProposedBlocks();
  for (auto i = 0; i < SYNC_TIMEOUT; i++) {
    if (num_proposed_blk > 0) {
      break;
    }
    taraxa::thisThreadSleepForMilliSeconds(500);
  }

  work.reset();
  node1->stop();
  EXPECT_GT(node1->getNumProposedBlocks(), 0);
}

TEST_F(FullNodeTest, DISABLED_sortition_propose_one_node) {
  val_t init_bal = 9007199254740991;
  FullNodeConfig cfg("./core_tests/conf/conf_taraxa1.json");
  cfg.test_params.block_proposer = {1, 1, 100, 4294967295};  // 2^32
  cfg.genesis_state.accounts[addr(cfg.node_secret)] = {init_bal};
  boost::asio::io_context context1;
  auto node1(std::make_shared<taraxa::FullNode>(context1, cfg));
  node1->setDebug(true);
  // destroy db !!
  node1->destroyDB();
  node1->start(true);  // boot node
  auto rpc = std::make_shared<ModularServer<dev::rpc::TestFace>>(
      new dev::rpc::Test(node1));
  auto rpc_server(
      std::make_shared<taraxa::RpcServer>(context1, cfg.rpc, node1));
  rpc->addConnector(rpc_server);
  rpc_server->StartListening();
  taraxa::thisThreadSleepForMilliSeconds(500);
  auto addr = node1->getAddress();
  auto res = node1->getBalance(addr);
  EXPECT_TRUE(res.second);
  EXPECT_EQ(res.first, init_bal);
  node1->setBlockProposeThresholdBeta(2048);

  try {
    sendTrx(1000, 7777);
  } catch (std::exception &e) {
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

  node1->stop();
  rpc->StopListening();
  EXPECT_GT(node1->getNumProposedBlocks(), 5);
}

TEST_F(TopTest, DISABLED_sortition_propose_five_nodes) {
  const char *input1[] = {
      "./build/main",
      "--conf_taraxa",
      "./core_tests/conf/sortition_propose_block/conf_taraxa1.json",
      "-v",
      "0",
      "--destroy_db"};
  const char *input2[] = {
      "./build/main2",
      "--conf_taraxa",
      "./core_tests/conf/sortition_propose_block/conf_taraxa2.json",
      "-v",
      "0",
      "--destroy_db"};
  const char *input3[] = {
      "./build/main3",
      "--conf_taraxa",
      "./core_tests/conf/sortition_propose_block/conf_taraxa3.json",
      "-v",
      "0",
      "--destroy_db"};
  const char *input4[] = {
      "./build/main4",
      "--conf_taraxa",
      "./core_tests/conf/sortition_propose_block/conf_taraxa4.json",
      "-v",
      "0",
      "--destroy_db"};
  const char *input5[] = {
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
  } catch (std::exception &e) {
    std::cerr << e.what() << std::endl;
  }
  Top top1(6, input1);
  EXPECT_TRUE(top1.isActive());
  auto node1 = top1.getNode();
  // node1->setBalance(node1->getAddress(), bal);
  taraxa::thisThreadSleepForMilliSeconds(1000);
  std::cout << "Top1 created ..." << std::endl;

  Top top2(6, input2);
  EXPECT_TRUE(top2.isActive());
  auto node2 = top2.getNode();

  std::cout << "Top2 created ..." << std::endl;

  Top top3(6, input3);
  EXPECT_TRUE(top3.isActive());
  auto node3 = top3.getNode();
  std::cout << "Top3 created ..." << std::endl;

  Top top4(6, input4);
  EXPECT_TRUE(top4.isActive());
  auto node4 = top4.getNode();
  std::cout << "Top4 created ..." << std::endl;

  Top top5(6, input5);
  EXPECT_TRUE(top5.isActive());
  std::cout << "Top5 created ..." << std::endl;
  auto node5 = top5.getNode();

  // set balance
  //  val_t bal(9007199254740991);
  // transfer some coins to your friends ...
  Transaction trx1to2(0, 0, val_t(0), samples::TEST_TX_GAS_LIMIT,
                      addr_t("973ecb1c08c8eb5a7eaa0d3fd3aab7924f2838b0"),
                      bytes(), samples::TX_GEN.getRandomUniqueSenderSecret());
  Transaction trx1to3(1, 0, val_t(0), samples::TEST_TX_GAS_LIMIT,
                      addr_t("4fae949ac2b72960fbe857b56532e2d3c8418d5e"),
                      bytes(), samples::TX_GEN.getRandomUniqueSenderSecret());
  Transaction trx1to4(2, 0, val_t(0), samples::TEST_TX_GAS_LIMIT,
                      addr_t("415cf514eb6a5a8bd4d325d4874eae8cf26bcfe0"),
                      bytes(), samples::TX_GEN.getRandomUniqueSenderSecret());
  Transaction trx1to5(3, 0, val_t(0), samples::TEST_TX_GAS_LIMIT,
                      addr_t("b770f7a99d0b7ad9adf6520be77ca20ee99b0858"),
                      bytes(), samples::TX_GEN.getRandomUniqueSenderSecret());
  node1->insertTransaction(trx1to2);
  node1->insertTransaction(trx1to3);
  node1->insertTransaction(trx1to4);
  node1->insertTransaction(trx1to5);
  // wait for top2, top3, top4, top5 initialize
  taraxa::thisThreadSleepForMilliSeconds(3000);

  // send 1000 trxs
  try {
    std::string sendtrx1 =
        R"(curl -m 10 -s -d '{"jsonrpc": "2.0", "id": "0", "method":
"create_test_coin_transactions", "params": [{ "secret":
"3800b2875669d9b2053c1aff9224ecfdc411423aac5b5a73d7a45ced1c3b9dcd", "delay": 5,
                                      "number": 6000,
                                      "nonce": 1,
                                      "receiver":"973ecb1c08c8eb5a7eaa0d3fd3aab7924f2838b0"
}]}' 0.0.0.0:7777)";
    std::string sendtrx2 = R"(curl -m 10 -s -d '{"jsonrpc":
"2.0", "id": "0", "method": "create_test_coin_transactions", "params": [{
"secret": "e6af8ca3b4074243f9214e16ac94831f17be38810d09a3edeb56ab55be848a1e",
                                      "delay": 7,
                                      "number": 4000,
                                      "nonce": 2,
                                      "receiver":"4fae949ac2b72960fbe857b56532e2d3c8418d5e"
}]}' 0.0.0.0:7778)";
    std::string sendtrx3 = R"(curl -m 10 -s -d '{"jsonrpc":
"2.0", "id": "0", "method": "create_test_coin_transactions", "params": [{
"secret": "f1261c9f09b0b483486c3b298f7c1ee001ff37e10023596528af93e34ba13f5f",
                                      "delay": 3,
                                      "number": 3000,
                                      "nonce": 3,
                                      "receiver":"415cf514eb6a5a8bd4d325d4874eae8cf26bcfe0"
}]}' 0.0.0.0:7779)";
    std::string sendtrx4 = R"(curl -m 10 -s -d '{"jsonrpc":
"2.0", "id": "0", "method": "create_test_coin_transactions", "params": [{
"secret": "7f38ee36812f2e4b1d75c9d21057fd718b9e7903ee9f9d4eb93b690790bb4029",
                                      "delay": 10,
                                      "number": 3000,
                                      "nonce": 4,
                                      "receiver":"b770f7a99d0b7ad9adf6520be77ca20ee99b0858"
}]}' 0.0.0.0:7780)";
    std::string sendtrx5 = R"(curl -m 10 -s -d '{"jsonrpc":
"2.0", "id": "0", "method": "create_test_coin_transactions", "params": [{
"secret": "beb2ed10f80e3feaf971614b2674c7de01cfd3127faa1bd055ed50baa1ce34fe",
                                      "delay": 2,
                                      "number": 4000,
                                      "nonce": 5,
                                      "receiver":"d79b2575d932235d87ea2a08387ae489c31aa2c9"
}]}' 0.0.0.0:7781)";
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

  } catch (std::exception &e) {
    std::cerr << e.what() << std::endl;
  }

  auto num_peers1 = node1->getPeerCount();
  auto num_peers2 = node2->getPeerCount();
  auto num_peers3 = node3->getPeerCount();
  auto num_peers4 = node4->getPeerCount();
  auto num_peers5 = node5->getPeerCount();

  for (auto i = 0; i < SYNC_TIMEOUT; i++) {
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

  for (auto i = 0; i < SYNC_TIMEOUT; i++) {
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
        node1->getTransactionStatusCount() == 20004 &&
        node2->getTransactionStatusCount() == 20004 &&
        node3->getTransactionStatusCount() == 20004 &&
        node4->getTransactionStatusCount() == 20004 &&
        node5->getTransactionStatusCount() == 20004)
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

  EXPECT_EQ(node1->getTransactionStatusCount(), 20004);
  EXPECT_EQ(node2->getTransactionStatusCount(), 20004);
  EXPECT_EQ(node3->getTransactionStatusCount(), 20004);
  EXPECT_EQ(node4->getTransactionStatusCount(), 20004);
  EXPECT_EQ(node5->getTransactionStatusCount(), 20004);

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
  } catch (std::exception &e) {
    std::cerr << e.what() << std::endl;
  }
}

TEST_F(TopTest, detect_overlap_transactions) {
  try {
    std::cout << "Copying main2 ..." << std::endl;
    system("cp ./build/main ./build/main2");
    std::cout << "Copying main3 ..." << std::endl;
    system("cp ./build/main ./build/main3");
    std::cout << "Copying main4 ..." << std::endl;
    system("cp ./build/main ./build/main4");
    std::cout << "Copying main5 ..." << std::endl;
    system("cp ./build/main ./build/main5");
  } catch (std::exception &e) {
    std::cerr << e.what() << std::endl;
  }

  Top top1(6, input1);
  EXPECT_TRUE(top1.isActive());
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

  std::cout << "Sleep for 1 second ..." << std::endl;

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

  node1->getPbftManager()->stop();
  node2->getPbftManager()->stop();
  node3->getPbftManager()->stop();
  node4->getPbftManager()->stop();
  node5->getPbftManager()->stop();

  EXPECT_GT(node1->getPeerCount(), 0);
  EXPECT_GT(node2->getPeerCount(), 0);
  EXPECT_GT(node3->getPeerCount(), 0);
  EXPECT_GT(node4->getPeerCount(), 0);
  EXPECT_GT(node5->getPeerCount(), 0);

  try {
    send_5_nodes_trxs(false);
  } catch (std::exception &e) {
    std::cerr << e.what() << std::endl;
  }
  // check dags
  auto num_vertices1 = node1->getNumVerticesInDag();
  auto num_vertices2 = node2->getNumVerticesInDag();
  auto num_vertices3 = node3->getNumVerticesInDag();
  auto num_vertices4 = node4->getNumVerticesInDag();
  auto num_vertices5 = node5->getNumVerticesInDag();

  for (auto i = 0; i < SYNC_TIMEOUT; i++) {
    num_vertices1 = node1->getNumVerticesInDag();
    num_vertices2 = node2->getNumVerticesInDag();
    num_vertices3 = node3->getNumVerticesInDag();
    num_vertices4 = node4->getNumVerticesInDag();
    num_vertices5 = node5->getNumVerticesInDag();

    auto trx_table = node1->getUnsafeTransactionStatusTable();
    int packed_trx = 0;

    if (trx_table.size() == 10000) {
      for (auto const &t : trx_table) {
        if (t.second == TransactionStatus::in_block) {
          packed_trx++;
        }
      }

      if (packed_trx == 10000 && num_vertices1 == num_vertices2 &&
          num_vertices2 == num_vertices3 && num_vertices3 == num_vertices4 &&
          num_vertices4 == num_vertices5 &&
          node1->getTransactionStatusCount() == 10000 &&
          node2->getTransactionStatusCount() == 10000 &&
          node3->getTransactionStatusCount() == 10000 &&
          node4->getTransactionStatusCount() == 10000 &&
          node5->getTransactionStatusCount() == 10000) {
        break;
      }
      if (i % 10 == 0) {
        std::cout << "Wait for vertices syncing ... packed trx " << packed_trx
                  << std::endl;
        std::cout << "Node 1 trx received: "
                  << node1->getTransactionStatusCount()
                  << " Dag size: " << num_vertices1 << std::endl;
        std::cout << "Node 2 trx received: "
                  << node2->getTransactionStatusCount()
                  << " Dag size: " << num_vertices2 << std::endl;
        std::cout << "Node 3 trx received: "
                  << node3->getTransactionStatusCount()
                  << " Dag size: " << num_vertices3 << std::endl;
        std::cout << "Node 4 trx received: "
                  << node4->getTransactionStatusCount()
                  << " Dag size: " << num_vertices4 << std::endl;
        std::cout << "Node 5 trx received: "
                  << node5->getTransactionStatusCount()
                  << " Dag size: " << num_vertices5 << std::endl;
      }
    }

    taraxa::thisThreadSleepForMilliSeconds(200);
  }

  EXPECT_GT(node1->getNumProposedBlocks(), 2);
  EXPECT_GT(node2->getNumProposedBlocks(), 2);
  EXPECT_GT(node3->getNumProposedBlocks(), 2);
  EXPECT_GT(node4->getNumProposedBlocks(), 2);
  EXPECT_GT(node5->getNumProposedBlocks(), 2);

  ASSERT_EQ(node1->getTransactionStatusCount(), 10000);
  ASSERT_EQ(node2->getTransactionStatusCount(), 10000);
  ASSERT_EQ(node3->getTransactionStatusCount(), 10000);
  ASSERT_EQ(node4->getTransactionStatusCount(), 10000);
  ASSERT_EQ(node5->getTransactionStatusCount(), 10000);

  // send dummy trx to make sure all DAGs are ordered
  try {
    send_dummy_trx(false);
  } catch (std::exception &e) {
    std::cerr << e.what() << std::endl;
  }

  for (int i = 0; i < SYNC_TIMEOUT; ++i) {
    num_vertices1 = node1->getNumVerticesInDag();
    num_vertices2 = node2->getNumVerticesInDag();
    num_vertices3 = node3->getNumVerticesInDag();
    num_vertices4 = node4->getNumVerticesInDag();
    num_vertices5 = node5->getNumVerticesInDag();
    if (i % 10 == 0) {
      std::cout << "Wait for dummy trx syncing ..." << std::endl;
      std::cout << "Node 1 trx received: " << node1->getTransactionStatusCount()
                << " Dag size: " << num_vertices1 << std::endl;
      std::cout << "Node 2 trx received: " << node2->getTransactionStatusCount()
                << " Dag size: " << num_vertices2 << std::endl;
      std::cout << "Node 3 trx received: " << node3->getTransactionStatusCount()
                << " Dag size: " << num_vertices3 << std::endl;
      std::cout << "Node 4 trx received: " << node4->getTransactionStatusCount()
                << " Dag size: " << num_vertices4 << std::endl;
      std::cout << "Node 5 trx received: " << node5->getTransactionStatusCount()
                << " Dag size: " << num_vertices5 << std::endl;
    }

    if (node1->getTransactionStatusCount() == 10001 &&
        node2->getTransactionStatusCount() == 10001 &&
        node3->getTransactionStatusCount() == 10001 &&
        node4->getTransactionStatusCount() == 10001 &&
        node5->getTransactionStatusCount() == 10001 &&
        num_vertices1 == num_vertices2 && num_vertices2 == num_vertices3 &&
        num_vertices3 == num_vertices4 && num_vertices4 == num_vertices5)
      break;
    taraxa::thisThreadSleepForMilliSeconds(200);
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

  EXPECT_EQ(node1->getTransactionStatusCount(), 10001);
  EXPECT_EQ(node2->getTransactionStatusCount(), 10001);
  EXPECT_EQ(node3->getTransactionStatusCount(), 10001);
  EXPECT_EQ(node4->getTransactionStatusCount(), 10001);
  EXPECT_EQ(node5->getTransactionStatusCount(), 10001);

  auto block = node1->getConfig().genesis_state.block;
  std::vector<std::string> ghost;
  node1->getGhostPath(block.getHash().toString(), ghost);
  ASSERT_GT(ghost.size(), 1);
  uint64_t period = 0, cur_period;
  std::shared_ptr<vec_blk_t> order;
  auto anchor = blk_hash_t(ghost.back());
  std::tie(cur_period, order) = node1->getDagBlockOrder(anchor);
  ASSERT_TRUE(order);
  EXPECT_GT(order->size(), 5);
  std::cout << "Ordered dagblock size: " << order->size() << std::endl;

  auto dag_size = node1->getNumVerticesInDag();

  // can have multiple dummy blocks
  auto vertices_diff = node1->getNumVerticesInDag().first - 1 - order->size();
  if (vertices_diff < 0 || vertices_diff >= 5) {
    node1->drawGraph("debug_dag");
    for (auto i(0); i < order->size(); ++i) {
      auto blk = (*order)[i];
      std::cout << i << " " << blk << " "
                << " trx: " << node1->getDagBlock(blk)->getTrxs().size()
                << std::endl;
    }
  }

  // diff should be larger than 0 but smaller than number of nodes
  // genesis block won't be executed
  EXPECT_LT(vertices_diff, 5)
      << " Number of vertices: " << node1->getNumVerticesInDag().first
      << " Number of ordered blks: " << order->size() << std::endl;
  EXPECT_GE(vertices_diff, 0)
      << " Number of vertices: " << node1->getNumVerticesInDag().first
      << " Number of ordered blks: " << order->size() << std::endl;

  auto overlap_table = node1->computeTransactionOverlapTable(order);
  // check transaction overlapping ...
  auto trx_table = node1->getUnsafeTransactionStatusTable();
  auto trx_table2 = trx_table;
  std::unordered_set<trx_hash_t> ordered_trxs;
  std::unordered_set<trx_hash_t> packed_trxs;
  uint all_trxs = 0;
  for (auto const &entry : *overlap_table) {
    auto const &blk = entry.first;
    auto const &overlap_vec = entry.second;
    auto dag_blk = node1->getDagBlock(blk);
    ASSERT_NE(dag_blk, nullptr);
    auto trxs = dag_blk->getTrxs();
    ASSERT_TRUE(trxs.size() == overlap_vec.size());
    all_trxs += overlap_vec.size();

    for (auto i = 0; i < trxs.size(); i++) {
      auto trx = trxs[i];
      if (!ordered_trxs.count(trx)) {
        EXPECT_TRUE(overlap_vec[i]);
        ordered_trxs.insert(trx);
        auto trx_status = trx_table[trx];
        EXPECT_EQ(trx_status, TransactionStatus::in_block)
            << "Error: " << trx << " status " << asInteger(trx_status)
            << std::endl;

      } else {
        EXPECT_FALSE(overlap_vec[i]);
      }
      auto it = trx_table2.find(trx);
      if (it != trx_table2.end()) {
        trx_table2.erase(it);
      }
      packed_trxs.insert(trx);
    }
  }
  for (auto const &t : trx_table2) {
    auto trx = node1->getTransaction(t.first);
    assert(trx);
    std::cout << "Unpacked trx: " << (*trx).first.getHash()
              << " sender: " << (*trx).first.getSender()
              << " nonce: " << (*trx).first.getNonce() << std::endl;
  }

  std::cout << "Total packed (overlapped) trxs: " << all_trxs << std::endl;
  EXPECT_GT(all_trxs, 10001);
  ASSERT_EQ(ordered_trxs.size(), 10001)
      << "Number of unpacked_trx " << trx_table2.size() << std::endl
      << "Total packed (non-overlapped) trxs " << packed_trxs.size()
      << std::endl;

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
  } catch (std::exception &e) {
    std::cerr << e.what() << std::endl;
  }
}

TEST_F(TopTest, transfer_to_self) {
  Top top1(6, input1);
  EXPECT_TRUE(top1.isActive());
  thisThreadSleepForMilliSeconds(500);
  std::cout << "Top1 created ..." << std::endl;
  auto node1 = top1.getNode();
  EXPECT_NE(node1, nullptr);
  std::cout << "Send first trx ..." << std::endl;
  auto node_addr = node1->getAddress();
  auto initial_bal = node1->getBalance(node_addr);
  auto trx_count(100);
  EXPECT_TRUE(initial_bal.second);
  system(fmt(R"(curl -m 10 -s -d '{"jsonrpc": "2.0", "id": "0",
  "method": "create_test_coin_transactions",
  "params": [{
    "secret":"3800b2875669d9b2053c1aff9224ecfdc411423aac5b5a73d7a45ced1c3b9dcd",
    "delay": 5,
    "number": %s,
    "nonce": 0,
    "receiver": "%s"}]}' 0.0.0.0:7777)",
             trx_count, node_addr)
             .data());
  thisThreadSleepForSeconds(5);
  EXPECT_EQ(node1->getTransactionStatusCount(), trx_count);
  auto trx_executed1 = node1->getNumTransactionExecuted();
  for (auto i(0); i < SYNC_TIMEOUT; ++i) {
    trx_executed1 = node1->getNumTransactionExecuted();
    if (trx_executed1 == trx_count) break;
    thisThreadSleepForMilliSeconds(100);
  }
  EXPECT_EQ(trx_executed1, trx_count);
  auto const bal = node1->getBalance(node_addr);
  EXPECT_TRUE(bal.second);
  EXPECT_EQ(bal.first, initial_bal.first);
}

TEST_F(TopTest, DISABLED_transaction_failure_does_not_cause_block_failure) {
  // TODO move to another file
  using namespace std;
  for (auto i(0); i < 2; ++i) {
    boost::asio::io_context io_ctx;
    FullNodeConfig conf("./core_tests/conf/conf_taraxa1.json");
    conf.use_basic_executor = i % 2 == 0;
    auto node = make_shared<FullNode>(io_ctx, conf);
    node->setDebug(true);
    node->start(true);
    thisThreadSleepForMilliSeconds(500);
    vector<Transaction> transactions;
    transactions.emplace_back(0, 100, 0, samples::TEST_TX_GAS_LIMIT, addr(),
                              bytes(), node->getSecretKey());
    // This must cause out of balance error
    transactions.emplace_back(1, node->getMyBalance() + 100, 0,
                              samples::TEST_TX_GAS_LIMIT, addr(), bytes(),
                              node->getSecretKey());
    for (auto const &trx : transactions) {
      node->insertTransaction(trx);
    }
    auto trx_executed = node->getNumTransactionExecuted();
    for (auto i(0); i < SYNC_TIMEOUT; ++i) {
      if (trx_executed == transactions.size()) break;
      thisThreadSleepForMilliSeconds(100);
      trx_executed = node->getNumTransactionExecuted();
    }
    EXPECT_EQ(trx_executed, transactions.size())
        << "use basic executor: " << conf.use_basic_executor
        << " ,Trx executed: " << trx_executed
        << " ,Trx size: " << transactions.size();
  }
}

}  // namespace taraxa

int main(int argc, char **argv) {
  TaraxaStackTrace st;
  dev::LoggingOptions logOptions;
  logOptions.verbosity = dev::VerbosityError;
  // logOptions.includeChannels.push_back("FULLND");
  // logOptions.includeChannels.push_back("DAGMGR");
  // logOptions.includeChannels.push_back("EXETOR");
  // logOptions.includeChannels.push_back("BLK_PP");
  // logOptions.includeChannels.push_back("PR_MDL");
  // logOptions.includeChannels.push_back("PBFT_MGR");
  // logOptions.includeChannels.push_back("PBFT_CHAIN");

  dev::setupLogging(logOptions);
  // use the in-memory db so test will not affect other each other through
  // persistent storage
  dev::db::setDatabaseKind(dev::db::DatabaseKind::MemoryDB);
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
