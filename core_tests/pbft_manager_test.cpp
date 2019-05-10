/*
 * @Copyright: Taraxa.io
 * @Author: Qi Gao
 * @Date: 2019-05-08
 * @Last Modified by:
 * @Last Modified time:
 */

#include "pbft_manager.hpp"

#include <gtest/gtest.h>

#include "full_node.hpp"
#include "network.hpp"

namespace taraxa {

TEST(PbftManager, pbft_manager_workflow) {
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

  bal_t new_balance = 9007199254740991;  // Max Taraxa coins 2^53 - 1
  addr_t account_address1 = node1->getAddress();
  node1->setBalance(account_address1, new_balance);
  node2->setBalance(account_address1, new_balance);
  node3->setBalance(account_address1, new_balance);
  addr_t account_address2 = node2->getAddress();
  node1->setBalance(account_address2, new_balance);
  node2->setBalance(account_address2, new_balance);
  node3->setBalance(account_address2, new_balance);
  addr_t account_address3 = node3->getAddress();
  node1->setBalance(account_address3, new_balance);
  node2->setBalance(account_address3, new_balance);
  node3->setBalance(account_address3, new_balance);

  std::shared_ptr<PbftManager> pbft_mgr1 = node1->getPbftManager();
  std::shared_ptr<PbftManager> pbft_mgr2 = node2->getPbftManager();
  std::shared_ptr<PbftManager> pbft_mgr3 = node3->getPbftManager();
  // period 1, step 1
  // node1 propose pbft anchor block, 1 propose vote
  pbft_mgr1->setPbftStep(1);
  pbft_mgr1->setPbftPeriod(1);
  pbft_mgr1->start();

  for (int i = 0; i < 3000; i++) {
    // test timeout is 3 seconds
    if (node1->getPbftQueueSize() == 1 && node1->getVoteQueueSize() == 1) {
      break;
    }
    taraxa::thisThreadSleepForMilliSeconds(1);
  }
  pbft_mgr1->stop();
  EXPECT_EQ(node1->getPbftQueueSize(), 1);
  EXPECT_EQ(node1->getVoteQueueSize(), 1);

  for (int i = 0; i < 300; i++) {
    // test timeout is 30 seconds
    if (node2->getPbftQueueSize() == 1 && node3->getPbftQueueSize() == 1) {
      break;
    }
    taraxa::thisThreadSleepForMilliSeconds(100);
  }
  EXPECT_EQ(node2->getPbftQueueSize(), 1);
  EXPECT_EQ(node3->getPbftQueueSize(), 1);

  for (int i = 0; i < 300; i++) {
    // test timeout is 30 seconds
    if (node2->getVoteQueueSize() == 1 && node3->getVoteQueueSize() == 1) {
      break;
    }
    taraxa::thisThreadSleepForMilliSeconds(100);
  }
  EXPECT_EQ(node2->getVoteQueueSize(), 1);
  EXPECT_EQ(node3->getVoteQueueSize(), 1);

  // period 1, step 2
  // node1 soft vote the pbft anchor block, 1 propose vote 1 soft vote
  pbft_mgr1->setPbftStep(2);
  pbft_mgr1->setPbftPeriod(1);
  pbft_mgr1->start();

  for (int i = 0; i < 3000; i++) {
    // test timeout is 3 seconds
    if (node1->getVoteQueueSize() == 2) {
      break;
    }
    taraxa::thisThreadSleepForMilliSeconds(1);
  }
  pbft_mgr1->stop();
  EXPECT_EQ(node1->getVoteQueueSize(), 2);

  for (int i = 0; i < 300; i++) {
    // test timeout is 30 seconds
    if (node2->getVoteQueueSize() == 2 && node3->getVoteQueueSize() == 2) {
      break;
    }
    taraxa::thisThreadSleepForMilliSeconds(100);
  }
  EXPECT_EQ(node2->getVoteQueueSize(), 2);
  EXPECT_EQ(node3->getVoteQueueSize(), 2);
  // node2 soft vote the pbft anchor block, 1 propose vote 2 soft votes
  pbft_mgr2->setPbftStep(2);
  pbft_mgr2->setPbftPeriod(1);
  pbft_mgr2->start();

  for (int i = 0; i < 3000; i++) {
    // test timeout is 3 seconds
    if (node2->getVoteQueueSize() == 3) {
      break;
    }
    taraxa::thisThreadSleepForMilliSeconds(1);
  }
  pbft_mgr2->stop();
  EXPECT_EQ(node2->getVoteQueueSize(), 3);

  for (int i = 0; i < 300; i++) {
    // test timeout is 30 seconds
    if (node1->getVoteQueueSize() == 3 && node3->getVoteQueueSize() == 3) {
      break;
    }
    taraxa::thisThreadSleepForMilliSeconds(100);
  }
  EXPECT_EQ(node1->getVoteQueueSize(), 3);
  EXPECT_EQ(node3->getVoteQueueSize(), 3);
  // node3 soft vote the pbft anchor block, 1 propose vote 3 soft votes
  pbft_mgr3->setPbftStep(2);
  pbft_mgr3->setPbftPeriod(1);
  pbft_mgr3->start();

  for (int i = 0; i < 3000; i++) {
    // test timeout is 3 seconds
    if (node3->getVoteQueueSize() == 4) {
      break;
    }
    taraxa::thisThreadSleepForMilliSeconds(1);
  }
  pbft_mgr3->stop();
  EXPECT_EQ(node3->getVoteQueueSize(), 4);

  for (int i = 0; i < 300; i++) {
    // test timeout is 30 seconds
    if (node1->getVoteQueueSize() == 4 && node2->getVoteQueueSize() == 4) {
      break;
    }
    taraxa::thisThreadSleepForMilliSeconds(100);
  }
  EXPECT_EQ(node1->getVoteQueueSize(), 4);
  EXPECT_EQ(node2->getVoteQueueSize(), 4);

  // period 1, step 3
  // node1 cert vote for the pbft anchor block, 1 propose vote 3 soft votes 1 cert vote
  pbft_mgr1->setPbftStep(3);
  pbft_mgr1->setPbftPeriod(1);
  pbft_mgr1->start();

  for (int i = 0; i < 3000; i++) {
    // test timeout is 3 seconds
    if (node1->getVoteQueueSize() == 5) {
      break;
    }
    taraxa::thisThreadSleepForMilliSeconds(1);
  }
  pbft_mgr1->stop();
  EXPECT_EQ(node1->getVoteQueueSize(), 5);

  for (int i = 0; i < 300; i++) {
    // test timeout is 30 seconds
    if (node2->getVoteQueueSize() == 5 && node3->getVoteQueueSize() == 5) {
      break;
    }
    taraxa::thisThreadSleepForMilliSeconds(100);
  }
  EXPECT_EQ(node2->getVoteQueueSize(), 5);
  EXPECT_EQ(node3->getVoteQueueSize(), 5);
  // node2 cert vote for the pbft anchor block, 1 propose vote 3 soft votes 2 cert votes
  pbft_mgr2->setPbftStep(3);
  pbft_mgr2->setPbftPeriod(1);
  pbft_mgr2->start();

  for (int i = 0; i < 3000; i++) {
    // test timeout is 3 seconds
    if (node2->getVoteQueueSize() == 6) {
      break;
    }
    taraxa::thisThreadSleepForMilliSeconds(1);
  }
  pbft_mgr2->stop();
  EXPECT_EQ(node2->getVoteQueueSize(), 6);

  for (int i = 0; i < 300; i++) {
    // test timeout is 30 seconds
    if (node1->getVoteQueueSize() == 6 && node3->getVoteQueueSize() == 6) {
      break;
    }
    taraxa::thisThreadSleepForMilliSeconds(100);
  }
  EXPECT_EQ(node1->getVoteQueueSize(), 6);
  EXPECT_EQ(node3->getVoteQueueSize(), 6);
  // node3 cert vote for the pbft anchor block, 1 propose vote 3 soft votes 3 cert votes
  pbft_mgr3->setPbftStep(3);
  pbft_mgr3->setPbftPeriod(1);
  pbft_mgr3->start();

  for (int i = 0; i < 3000; i++) {
    // test timeout is 3 seconds
    if (node3->getVoteQueueSize() == 7) {
      break;
    }
    taraxa::thisThreadSleepForMilliSeconds(1);
  }
  pbft_mgr3->stop();
  EXPECT_EQ(node3->getVoteQueueSize(), 7);

  for (int i = 0; i < 300; i++) {
    // test timeout is 30 seconds
    if (node1->getVoteQueueSize() == 7 && node2->getVoteQueueSize() == 7) {
      break;
    }
    taraxa::thisThreadSleepForMilliSeconds(100);
  }
  EXPECT_EQ(node1->getVoteQueueSize(), 7);
  EXPECT_EQ(node2->getVoteQueueSize(), 7);

  // period 1, step 4
  pbft_mgr1->setPbftStep(4);
  pbft_mgr1->setPbftPeriod(1);
  pbft_mgr1->start();
  // TODO

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
  logOptions.verbosity = dev::VerbosityDebug;
  logOptions.includeChannels.push_back("PBFT_MGR");
//  logOptions.includeChannels.push_back("TARCAP");
  dev::setupLogging(logOptions);
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}