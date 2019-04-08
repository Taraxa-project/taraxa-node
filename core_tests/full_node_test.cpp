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
#include "libdevcore/Log.h"
#include "network.hpp"
#include "rpc.hpp"

namespace taraxa {

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
  logOptions.includeChannels.push_back("RPC");

  dev::setupLogging(logOptions);
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}