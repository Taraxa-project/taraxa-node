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

TEST(PbftVote, transfer_vote) {
/*
  boost::asio::io_context context;
  auto node(std::make_shared<taraxa::FullNode>(
      context,
      std::string("./core_tests/conf_full_node1.json"),
      std::string("./core_tests/conf_network1.json")));
*/
  std::shared_ptr<Network> nw1(
      new taraxa::Network("./core_tests/conf_network1.json"));
  std::shared_ptr<Network> nw2(
      new taraxa::Network("./core_tests/conf_network2.json"));

  nw1->start();
  nw2->start();
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
//  node->setBalance(account_address, new_balance);

//  node->clearVoteQueue();
  taraxa::thisThreadSleepForSeconds(10);
  nw2->sendPbftVote(nw1->getNodeId(), vote);
  std::cout << "Waiting packages for 10 seconds ..." << std::endl;
  taraxa::thisThreadSleepForSeconds(10);

  nw2->stop();
  nw1->stop();

//  size_t vote_queue_size = node->getVoteQueueSize();
//  EXPECT_EQ(vote_queue_size, 1);
}

}  // namespace taraxa

int main(int argc, char** argv) {
  dev::LoggingOptions logOptions;
  logOptions.verbosity = dev::VerbosityDebug;
  dev::setupLogging(logOptions);
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
