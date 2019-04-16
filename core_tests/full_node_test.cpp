/*
 * @Copyright: Taraxa.io
 * @Author: Chia-Chun Lin
 * @Date: 2019-01-18 12:56:45
 * @Last Modified by: Chia-Chun Lin
 * @Last Modified time: 2019-04-08 15:59:24
 */

#include "full_node.hpp"
#include <gtest/gtest.h>
#include <atomic>
#include <boost/thread.hpp>
#include <iostream>
#include <vector>
#include "create_samples.hpp"
#include "dag.hpp"
#include "libdevcore/Log.h"
#include "network.hpp"
#include "pbft_chain.hpp"
#include "rpc.hpp"
#include "string"

namespace taraxa {

const unsigned NUM_TRX = 50;

auto g_secret = dev::Secret(
    "3800b2875669d9b2053c1aff9224ecfdc411423aac5b5a73d7a45ced1c3b9dcd",
    dev::Secret::ConstructFromStringType::FromHex);
auto g_key_pair = dev::KeyPair(g_secret);
auto g_trx_signed_samples =
    samples::createSignedTrxSamples(0, NUM_TRX, g_secret);

TEST(FullNode, account_bal) {
  boost::asio::io_context context;

  auto node(std::make_shared<taraxa::FullNode>(
      context, std::string("./core_tests/conf_full_node1.json"),
      std::string("./core_tests/conf_network1.json")));
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
      context, std::string("./core_tests/conf_full_node1.json"),
      std::string("./core_tests/conf_network1.json")));
  node->start();
  addr_t acc1 = node->getAddress();
  bal_t bal1(10000000);
  node->setBalance(acc1, bal1);
  auto res = node->getBalance(acc1);
  EXPECT_TRUE(res.second);
  EXPECT_EQ(res.first, bal1);
  for (auto const& t : g_trx_signed_samples) {
    node->storeTransaction(t);
  }

  taraxa::thisThreadSleepForMilliSeconds(1000);

  EXPECT_GT(node->getNumProposedBlocks(), NUM_TRX / 10 - 2);

  std::vector<std::string> ghost;
  node->getGhostPath(Dag::GENESIS, ghost);
  vec_blk_t blks;
  std::vector<std::vector<uint>> modes;
  for (auto const& g : ghost) {
    if (g == Dag::GENESIS) continue;
    auto dagblk = node->getDagBlock(blk_hash_t(g));
    std::vector<uint> mode(dagblk->getTrxs().size(), 1);
    blks.push_back(dagblk->getHash());
    modes.push_back(mode);
  }
  TrxSchedule sche(blks, modes);
  ScheduleBlock sche_blk(blk_hash_t(100), 12345, sig_t(200), sche);
  node->executeScheduleBlock(sche_blk);
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
      context1, std::string("./core_tests/conf_full_node1.json"),
      std::string("./core_tests/conf_network1.json")));

  // node1->setVerbose(true);
  node1->setDebug(true);
  node1->start();

  // send package
  auto nw2(
      std::make_shared<taraxa::Network>("./core_tests/conf_network2.json"));

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

  std::cout << "Waiting packages for 1000 milliseconds ..." << std::endl;
  taraxa::thisThreadSleepForMilliSeconds(1000);

  work.reset();
  nw2->stop();

  std::cout << "Waiting packages for 1000 milliseconds ..." << std::endl;
  taraxa::thisThreadSleepForMilliSeconds(1000);
  node1->stop();
  t.join();

  // node1->drawGraph("dot.txt");
  EXPECT_EQ(node1->getNumReceivedBlocks(), blks.size());
  EXPECT_EQ(node1->getNumVerticesInDag().first, 7);
  EXPECT_EQ(node1->getNumEdgesInDag().first, 8);
  EXPECT_EQ(node1->getNumProposedBlocks(), 0);
}

TEST(FullNode, receive_send_transaction) {
  boost::asio::io_context context1;

  auto node1(std::make_shared<taraxa::FullNode>(
      context1, std::string("./core_tests/conf_full_node1.json"),
      std::string("./core_tests/conf_network1.json")));
  auto rpc(std::make_shared<taraxa::Rpc>(
      context1, "./core_tests/conf_rpc1.json", node1->getShared()));
  rpc->start();
  node1->setDebug(true);
  node1->start();

  std::unique_ptr<boost::asio::io_context::work> work(
      new boost::asio::io_context::work(context1));

  boost::thread t([&context1]() { context1.run(); });

  try {
    system("./core_tests/curl1000_send_trx.sh");
  } catch (std::exception& e) {
    std::cerr << e.what() << std::endl;
  }
  std::cout << "1000 transaction are sent through RPC ..." << std::endl;

  taraxa::thisThreadSleepForMilliSeconds(500);

  work.reset();
  node1->stop();
  rpc->stop();
  t.join();
  EXPECT_EQ(node1->getNumProposedBlocks(), 100);
}

}  // namespace taraxa

int main(int argc, char** argv) {
  dev::LoggingOptions logOptions;
  logOptions.verbosity = dev::VerbosityError;
  logOptions.includeChannels.push_back("FULL_ND");
  // logOptions.includeChannels.push_back("EXETOR");
  // logOptions.includeChannels.push_back("trx_qu");
  // logOptions.includeChannels.push_back("trx_mgr");

  logOptions.includeChannels.push_back("RPC");

  dev::setupLogging(logOptions);
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}