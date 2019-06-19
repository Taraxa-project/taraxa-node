/*
 * @Copyright: Taraxa.io
 * @Author: Qi Gao
 * @Date: 2019-04-09
 * @Last Modified by: Qi Gao
 * @Last Modified time: 2019-05-12
 */
#include "pbft_manager.hpp"

#include "full_node.hpp"
#include "libdevcore/DBFactory.h"
#include "libdevcore/Log.h"
#include "libdevcore/SHA3.h"
#include "network.hpp"
#include "rpc.hpp"

#include <gtest/gtest.h>
#include <boost/thread.hpp>

namespace taraxa {

TEST(PbftManager, pbft_manager_lambda_input_test) {
  PbftManagerConfig pbft_config;
  pbft_config.lambda_ms = 1000;
  PbftManager pbft_manager(pbft_config);
  u_long lambda = pbft_manager.getLambdaMs();
  EXPECT_EQ(lambda, pbft_config.lambda_ms);
}

TEST(PbftManager, full_node_lambda_input_test) {
  boost::asio::io_context context;
  auto node(std::make_shared<taraxa::FullNode>(
      context, std::string("./core_tests/conf_taraxa1.json")));
  auto pbft_mgr = node->getPbftManager();
  u_long lambda = pbft_mgr->getLambdaMs();
  EXPECT_EQ(lambda, 1000);
}

TEST(PbftVote, pbft_should_speak_test) {
  boost::asio::io_context context;
  FullNodeConfig conf("./core_tests/conf_taraxa1.json");
  auto node(std::make_shared<taraxa::FullNode>(context, conf));
  auto rpc(std::make_shared<taraxa::Rpc>(context, conf.rpc, node->getShared()));
  rpc->start();
  node->setDebug(true);
  node->start(true); // boot node

  std::unique_ptr<boost::asio::io_context::work> work(
      new boost::asio::io_context::work(context));

  boost::thread t([&context]() { context.run(); });

  try {
    system("./core_tests/curl_pbft_should_speak.sh");
  } catch (std::exception& e) {
    std::cerr << e.what() << std::endl;
  }

  work.reset();
  node->stop();
  rpc->stop();
  t.join();
}

// Place votes period 1, 2 and 3 into vote queue.
// Get vote period 2, will remove period 1 in the queue. Queue size changes
// to 2.
TEST(PbftVote, pbft_place_and_get_vote_test) {
  boost::asio::io_context context;

  FullNodeConfig conf("./core_tests/conf_taraxa1.json");
  auto node(std::make_shared<taraxa::FullNode>(context, conf));
  auto rpc(std::make_shared<taraxa::Rpc>(context, conf.rpc, node->getShared()));
  rpc->start();
  node->setDebug(true);
  node->start(true); // boot node

  std::unique_ptr<boost::asio::io_context::work> work(
      new boost::asio::io_context::work(context));

  boost::thread t([&context]() { context.run(); });

  node->clearVoteQueue();

  try {
    system("./core_tests/curl_pbft_place_vote.sh");
  } catch (std::exception& e) {
    std::cerr << e.what() << std::endl;
  }

  try {
    system("./core_tests/curl_pbft_get_votes.sh");
  } catch (std::exception& e) {
    std::cerr << e.what() << std::endl;
  }

  work.reset();
  node->stop();
  rpc->stop();
  t.join();

  size_t vote_queue_size = node->getVoteQueueSize();
  EXPECT_EQ(vote_queue_size, 2);
}

// Generate a vote, send the vote from node2 to node1
TEST(PbftVote, transfer_vote) {
  boost::asio::io_context context1;
  auto node1(std::make_shared<taraxa::FullNode>(
      context1, std::string("./core_tests/conf_taraxa1.json")));
  boost::asio::io_context context2;
  auto node2(std::make_shared<taraxa::FullNode>(
      context2, std::string("./core_tests/conf_taraxa2.json")));

  node1->setDebug(true);
  node2->setDebug(true);
  node1->start(true); // boot node
  node2->start(false);

  std::unique_ptr<boost::asio::io_context::work> work1(
      new boost::asio::io_context::work(context1));
  std::unique_ptr<boost::asio::io_context::work> work2(
      new boost::asio::io_context::work(context2));

  boost::thread t1([&context1]() { context1.run(); });
  boost::thread t2([&context2]() { context2.run(); });

  std::shared_ptr<Network> nw1 = node1->getNetwork();
  std::shared_ptr<Network> nw2 = node2->getNetwork();

  int node_peers = 1;
  for (int i = 0; i < 300; i++) {
    // test timeout is 30 seconds
    if (nw1->getPeerCount() == node_peers &&
        nw2->getPeerCount() == node_peers) {
      break;
    }
    taraxa::thisThreadSleepForMilliSeconds(100);
  }
  ASSERT_EQ(node_peers, nw1->getPeerCount());
  ASSERT_EQ(node_peers, nw2->getPeerCount());

  // set nodes account balance
  bal_t new_balance = 9007199254740991;  // Max Taraxa coins 2^53 - 1
  addr_t account_address1 = node1->getAddress();
  node1->setBalance(account_address1, new_balance);
  node2->setBalance(account_address1, new_balance);
  addr_t account_address2 = node2->getAddress();
  node1->setBalance(account_address2, new_balance);
  node2->setBalance(account_address2, new_balance);

  // stop PBFT manager, that will place vote
  std::shared_ptr<PbftManager> pbft_mgr1 = node1->getPbftManager();
  std::shared_ptr<PbftManager> pbft_mgr2 = node2->getPbftManager();
  pbft_mgr1->stop();
  pbft_mgr2->stop();

  // generate vote
  blk_hash_t blockhash(1);
  PbftVoteTypes type = propose_vote_type;
  uint64_t period = 1;
  size_t step = 1;
  Vote vote = node2->generateVote(blockhash, type, period, step);

  node1->clearVoteQueue();
  node2->clearVoteQueue();

  nw2->sendPbftVote(nw1->getNodeId(), vote);

  work1.reset();
  work2.reset();
  node1->stop();
  node2->stop();
  t1.join();
  t2.join();

  size_t vote_queue_size = node1->getVoteQueueSize();
  EXPECT_EQ(vote_queue_size, 1);
}

TEST(PbftVote, vote_broadcast) {
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
  node1->start(true); // boot node
  node2->start(false);
  node3->start(false);

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

  int node_peers = 2;
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

  // set nodes account balance
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

  // stop PBFT manager, that will place vote
  std::shared_ptr<PbftManager> pbft_mgr1 = node1->getPbftManager();
  std::shared_ptr<PbftManager> pbft_mgr2 = node2->getPbftManager();
  std::shared_ptr<PbftManager> pbft_mgr3 = node3->getPbftManager();
  pbft_mgr1->stop();
  pbft_mgr2->stop();
  pbft_mgr3->stop();

  // generate vote
  blk_hash_t blockhash(1);
  PbftVoteTypes type = propose_vote_type;
  uint64_t period = 1;
  size_t step = 1;
  Vote vote = node1->generateVote(blockhash, type, period, step);

  node1->clearVoteQueue();
  node2->clearVoteQueue();
  node3->clearVoteQueue();

  nw1->onNewPbftVote(vote);

  work1.reset();
  work2.reset();
  work3.reset();
  node1->stop();
  node2->stop();
  node3->stop();
  t1.join();
  t2.join();
  t3.join();

  size_t vote_queue_size1 = node1->getVoteQueueSize();
  size_t vote_queue_size2 = node2->getVoteQueueSize();
  size_t vote_queue_size3 = node3->getVoteQueueSize();
  EXPECT_EQ(vote_queue_size1, 0);
  EXPECT_EQ(vote_queue_size2, 1);
  EXPECT_EQ(vote_queue_size3, 1);
}

}  // namespace taraxa

int main(int argc, char** argv) {
  TaraxaStackTrace st;
  dev::LoggingOptions logOptions;
  logOptions.verbosity = dev::VerbositySilent;
  logOptions.includeChannels.push_back("NETWORK");
  logOptions.includeChannels.push_back("TARCAP");
  logOptions.includeChannels.push_back("VOTE_MGR");
  dev::setupLogging(logOptions);
  // use the in-memory db so test will not affect other each other through
  // persistent storage
  dev::db::setDatabaseKind(dev::db::DatabaseKind::MemoryDB);
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
