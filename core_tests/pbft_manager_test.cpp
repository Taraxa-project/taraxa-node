/*
 * @Copyright: Taraxa.io
 * @Author: Qi Gao
 * @Date: 2019-05-08
 * @Last Modified by:
 * @Last Modified time:
 */

#include "pbft_manager.hpp"

#include <gtest/gtest.h>
//#include <atomic>
//#include <boost/thread.hpp>
//#include <iostream>
//#include <vector>

#include "full_node.hpp"
//#include "libdevcore/Log.h"
#include "network.hpp"
//#include "pbft_chain.hpp"
//#include "util.hpp"

namespace taraxa {

TEST(PbftManager, propose_pbft_anchor_block) {
  boost::asio::io_context context1;
  auto node1(std::make_shared<taraxa::FullNode>(
  		context1, std::string("./core_tests/conf_taraxa1.json")));
  boost::asio::io_context context2;
  auto node2(std::make_shared<taraxa::FullNode>(
		context2, std::string("./core_tests/conf_taraxa2.json")));
  boost::asio::io_context context3;
  auto node3(std::make_shared<taraxa::FullNode>(
		context3, std::string("./core_tests/conf_taraxa3.json")));
  node1->setDebug(true);
  node2->setDebug(true);
  node3->setDebug(true);
  node1->start(true /*boot_node*/);
  node2->start(false /*boot_node*/);
  node3->start(false /*boot_node*/);

  std::unique_ptr<boost::asio::io_context::work> work1(
  		new boost::asio::io_context::work(context1));
  std::unique_ptr<boost::asio::io_context::work> work2(
		new boost::asio::io_context::work(context2));
  std::unique_ptr<boost::asio::io_context::work> work3(
		new boost::asio::io_context::work(context3));

  boost::thread t1([&context1]() { context1.run(); });
  boost::thread t2([&context2]() { context2.run(); });
  boost::thread t3([&context3]() { context3.run(); });

  std::shared_ptr<Network> nw1 = node1->getNetwork();
  std::shared_ptr<Network> nw2 = node2->getNetwork();
  std::shared_ptr<Network> nw3 = node3->getNetwork();

  const int node_peers = 2;
  for (int i = 0; i < 300; i++) {
	// test timeout is 30 seconds
	if (nw1->getPeerCount() == node_peers &&
	    nw2->getPeerCount() == node_peers &&
	    nw3->getPeerCount() == node_peers) {
		break;
	}
	taraxa::thisThreadSleepForMilliSeconds(100);
  }
  ASSERT_EQ(node_peers, nw1->getPeerCount());
  ASSERT_EQ(node_peers, nw2->getPeerCount());
  ASSERT_EQ(node_peers, nw3->getPeerCount());

  std::shared_ptr<PbftManager> pbft_mgr = node1->getPbftManager();
  pbft_mgr->setPbftStep(1);
  pbft_mgr->setPbftPeriod(1);
  pbft_mgr->start();

  int current_pbft_queue_size = 1;
  for (int i = 0; i < 300; i++) {
  	// test timeout is 30 seconds
  	if (node2->getPbftQueueSize() == current_pbft_queue_size &&
  	    node3->getPbftQueueSize() == current_pbft_queue_size) {
  		break;
  	}
  	taraxa::thisThreadSleepForMilliSeconds(100);
  }
  EXPECT_EQ(node1->getPbftQueueSize(), current_pbft_queue_size);
  EXPECT_EQ(node2->getPbftQueueSize(), current_pbft_queue_size);
  EXPECT_EQ(node3->getPbftQueueSize(), current_pbft_queue_size);

  work1.reset();
  work2.reset();
  work3.reset();
  node1->stop();
  node2->stop();
  node3->stop();
  t1.join();
  t2.join();
  t3.join();
}

TEST(PbftManager, DISABLED_create_pbft_manager) {
  PbftManager pbft_mgr;
  pbft_mgr.start();
  thisThreadSleepForSeconds(1);
  EXPECT_TRUE(pbft_mgr.isActive());
  pbft_mgr.stop();
  EXPECT_FALSE(pbft_mgr.isActive());
}

} // namespace taraxa

int main(int argc, char** argv) {
  dev::LoggingOptions logOptions;
  logOptions.verbosity = dev::VerbosityError;
  logOptions.includeChannels.push_back("PBFT_MGR");
  dev::setupLogging(logOptions);
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}