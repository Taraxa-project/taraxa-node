/*
 * @Copyright: Taraxa.io
 * @Author: Qi Gao
 * @Date: 2019-04-09
 * @Last Modified by:
 * @Last Modified time:
 */
#include "full_node.hpp"
#include "libdevcore/Log.h"
#include "libdevcore/SHA3.h"
#include "network.hpp"
#include "rpc.hpp"

#include <boost/thread.hpp>
#include <gtest/gtest.h>

namespace taraxa {

TEST(PbftVote, pbft_should_speak_test) {
  boost::asio::io_context context;

  auto node(std::make_shared<taraxa::FullNode>(
      context,
      std::string("./core_tests/conf_full_node1.json"),
      std::string("./core_tests/conf_network1.json")));
  auto rpc(std::make_shared<taraxa::Rpc>(
      context, "./core_tests/conf_rpc1.json", node->getShared()));
  rpc->start();
  node->setDebug(true);
  node->start();

  std::unique_ptr<boost::asio::io_context::work> work(
      new boost::asio::io_context::work(context));

  boost::thread t([&context]() {
    context.run();
  });

  try {
    system("./core_tests/curl_pbft_should_speak.sh");
  } catch (std::exception& e) {
    std::cerr << e.what() << std::endl;
  }

  taraxa::thisThreadSleepForMilliSeconds(500);

  work.reset();
  node->stop();
  rpc->stop();
  t.join();
}

/* Place votes period 1, 2 and 3 into vote queue.
 * Get vote period 2, will remove period 1 in the queue. Queue size changes to 2.
 */
TEST(PbftVote, pbft_place_and_get_vote_test) {
  boost::asio::io_context context;

  auto node(std::make_shared<taraxa::FullNode>(
      context,
      std::string("./core_tests/conf_full_node1.json"),
      std::string("./core_tests/conf_network1.json")));
  auto rpc(std::make_shared<taraxa::Rpc>(
      context, "./core_tests/conf_rpc1.json", node->getShared()));
  rpc->start();
  node->setDebug(true);
  node->start();

  std::unique_ptr<boost::asio::io_context::work> work(
      new boost::asio::io_context::work(context));

  boost::thread t([&context]() {
    context.run();
  });

  node->clearVoteQueue();

  try {
    system("./core_tests/curl_pbft_place_vote.sh");
  } catch (std::exception& e) {
    std::cerr << e.what() << std::endl;
  }

  taraxa::thisThreadSleepForMilliSeconds(500);

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
      context1,
      std::string("./core_tests/conf_full_node1.json"),
      std::string("./core_tests/conf_network1.json")));
  boost::asio::io_context context2;
  auto node2(std::make_shared<taraxa::FullNode>(
      context2,
      std::string("./core_tests/conf_full_node2.json"),
      std::string("./core_tests/conf_network2.json")));

  node1->setDebug(true);
  node2->setDebug(true);
  node1->start();
  node2->start();

  std::unique_ptr<boost::asio::io_context::work> work1(
      new boost::asio::io_context::work(context1));
  std::unique_ptr<boost::asio::io_context::work> work2(
      new boost::asio::io_context::work(context2));

  boost::thread t1([&context1]() {
    context1.run();
  });
  boost::thread t2([&context2]() {
    context2.run();
  });

  std::shared_ptr<Network> nw1 = node1->getNetwork();
  std::shared_ptr<Network> nw2 = node2->getNetwork();

  // generate vote
  blk_hash_t blockhash(1);
  char type = '1';
  int period = 1;
  int step = 1;
  std::string message = blockhash.toString() +
                        std::to_string(type) +
                        std::to_string(period) +
                        std::to_string(step);
  dev::KeyPair key_pair = dev::KeyPair::create();
  dev::Signature signature = dev::sign(key_pair.secret(), dev::sha3(message));
  Vote vote(key_pair.pub(), signature, blockhash, type, period, step);

  addr_t account_address = dev::toAddress(key_pair.pub());
  bal_t new_balance = 9007199254740991; // Max Taraxa coins 2^53 - 1
  node1->setBalance(account_address, new_balance);

  node1->clearVoteQueue();

  taraxa::thisThreadSleepForSeconds(10);
  nw2->sendPbftVote(nw1->getNodeId(), vote);
  std::cout << "Waiting packages for 10 seconds ..." << std::endl;
  taraxa::thisThreadSleepForSeconds(10);

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
      context1,
      std::string("./core_tests/conf_full_node1.json"),
      std::string("./core_tests/conf_network1.json")));
  boost::asio::io_context context2;
  auto node2(std::make_shared<taraxa::FullNode>(
      context2,
      std::string("./core_tests/conf_full_node2.json"),
      std::string("./core_tests/conf_network2.json")));
  boost::asio::io_context context3;
  auto node3(std::make_shared<taraxa::FullNode>(
      context3,
      std::string("./core_tests/conf_full_node3.json"),
      std::string("./core_tests/conf_network3.json")));
  node1->setDebug(true);
  node2->setDebug(true);
  node3->setDebug(true);
  node1->start();
  node2->start();
  node3->start();

  std::unique_ptr<boost::asio::io_context::work> work1(
      new boost::asio::io_context::work(context1));
  std::unique_ptr<boost::asio::io_context::work> work2(
      new boost::asio::io_context::work(context2));
  std::unique_ptr<boost::asio::io_context::work> work3(
      new boost::asio::io_context::work(context3));

  boost::thread t1([&context1]() {
    context1.run();
  });
  boost::thread t2([&context2]() {
    context2.run();
  });
  boost::thread t3([&context3]() {
    context3.run();
  });

  std::shared_ptr<Network> nw1 = node1->getNetwork();
  std::shared_ptr<Network> nw2 = node2->getNetwork();
  std::shared_ptr<Network> nw3 = node3->getNetwork();

  taraxa::thisThreadSleepForSeconds(20);

  ASSERT_EQ(2, nw1->getPeerCount());
  ASSERT_EQ(2, nw2->getPeerCount());
  ASSERT_EQ(2, nw3->getPeerCount());

  // generate vote
  blk_hash_t blockhash(1);
  char type = '1';
  int period = 1;
  int step = 1;
  std::string message = blockhash.toString() +
                        std::to_string(type) +
                        std::to_string(period) +
                        std::to_string(step);
  dev::KeyPair key_pair = dev::KeyPair::create();
  dev::Signature signature = dev::sign(key_pair.secret(), dev::sha3(message));
  Vote vote(key_pair.pub(), signature, blockhash, type, period, step);

  addr_t account_address = dev::toAddress(key_pair.pub());
  bal_t new_balance = 9007199254740991; // Max Taraxa coins 2^53 - 1
  node1->setBalance(account_address, new_balance);
  node2->setBalance(account_address, new_balance);
  node3->setBalance(account_address, new_balance);

  node1->clearVoteQueue();
  node2->clearVoteQueue();
  node3->clearVoteQueue();

  taraxa::thisThreadSleepForSeconds(10);
  nw1->onNewPbftVote(vote);
  std::cout << "Waiting packages for 10 seconds ..." << std::endl;
  taraxa::thisThreadSleepForSeconds(10);

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
  dev::LoggingOptions logOptions;
  logOptions.verbosity = dev::VerbosityDebug;
  logOptions.includeChannels.push_back("network");
  dev::setupLogging(logOptions);
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
