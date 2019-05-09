/*
 * @Copyright: Taraxa.io
 * @Author: Chia-Chun Lin
 * @Date: 2019-04-08 15:59:01
 * @Last Modified by: Qi Gao
 * @Last Modified time: 2019-05-05
 */

#include "pbft_chain.hpp"
#include <gtest/gtest.h>
#include <atomic>
#include <boost/thread.hpp>
#include <iostream>
#include <vector>
#include "full_node.hpp"
#include "libdevcore/Log.h"
#include "network.hpp"
#include "pbft_manager.hpp"
#include "util.hpp"

namespace taraxa {

TEST(TrxSchedule, serialize_deserialize) {
  vec_blk_t blks{blk_hash_t(123), blk_hash_t(456), blk_hash_t(32443)};
  std::vector<std::vector<uint>> modes{
      {0, 1, 2, 0, 1, 2}, {1, 1, 1, 1, 1}, {0, 0, 0}};
  TrxSchedule sche1(blks, modes);
  auto rlp = sche1.rlp();
  TrxSchedule sche2(rlp);
  EXPECT_EQ(sche1, sche2);
}

TEST(PivotBlock, serialize_deserialize) {
  blk_hash_t prev_pivot_blk(34);
  blk_hash_t prev_res_blk(56);
  blk_hash_t dag_blk(78);
  uint64_t epoch = 1;
  uint64_t timestamp = 123456;
  addr_t beneficiary(10);
  PivotBlock pivot_block1(prev_pivot_blk, prev_res_blk, dag_blk, epoch,
      timestamp, beneficiary);

  std::stringstream ss1, ss2;
  ss1 << pivot_block1;
  std::vector<uint8_t> bytes;
  {
    vectorstream strm1(bytes);
    pivot_block1.serialize(strm1);
  }

  bufferstream strm2(bytes.data(), bytes.size());

  PivotBlock pivot_block2(strm2);
  ss2 << pivot_block2;

  // Compare PivotBlock content
  EXPECT_EQ(ss1.str(), ss2.str());
}

TEST(ScheduleBlock, serialize_deserialize) {
  blk_hash_t prev_pivot(22);
  uint64_t timestamp = 333333;
  vec_blk_t blks { blk_hash_t(123), blk_hash_t(456), blk_hash_t(789) };
  std::vector<std::vector<uint>> modes{
      {0, 1, 2, 0, 1, 2}, {1, 1, 1, 1, 1}, {0, 0, 0}};
  TrxSchedule schedule(blks, modes);
  ScheduleBlock schedule_blk1(prev_pivot, timestamp, schedule);

  std::stringstream ss1, ss2;
  ss1 << schedule_blk1;

  std::vector<uint8_t> bytes;
  {
    vectorstream strm1(bytes);
    schedule_blk1.serialize(strm1);
  }

  bufferstream strm2(bytes.data(), bytes.size());
  ScheduleBlock schedule_blk2(strm2);
  ss2 << schedule_blk2;

  // Compare PivotBlock content
  EXPECT_EQ(ss1.str(), ss2.str());
}

TEST(PbftChain, block_broadcast) {
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

  // generate pbft pivot block sample
  blk_hash_t prev_pivot_blk(0);
  blk_hash_t prev_res_blk(0);
  blk_hash_t dag_blk(78);
  uint64_t epoch = 1;
  uint64_t timestamp1 = 123456;
  addr_t beneficiary(10);
  PivotBlock pivot_block(prev_pivot_blk, prev_res_blk, dag_blk, epoch,
      timestamp1, beneficiary);
  PbftBlock pbft_block1(blk_hash_t(1));
  pbft_block1.setPivotBlock(pivot_block);

  node1->pushPbftBlockIntoQueue(pbft_block1);
  EXPECT_EQ(node1->getPbftQueueSize(), 1);
  node1->setPbftBlock(pbft_block1); // Test pbft chain
  EXPECT_EQ(node1->getPbftChainSize(), 2);

  nw1->onNewPbftBlock(pbft_block1);

  int current_pbft_queue_size = 1;
  for (int i = 0; i < 300; i++) {
    // test timeout is 30 seconds
    if (node2->getPbftQueueSize() == current_pbft_queue_size &&
        node3->getPbftQueueSize() == current_pbft_queue_size) {
      break;
    }
    taraxa::thisThreadSleepForMilliSeconds(100);
  }
  EXPECT_EQ(node2->getPbftQueueSize(), current_pbft_queue_size);
  EXPECT_EQ(node3->getPbftQueueSize(), current_pbft_queue_size);
  node2->setPbftBlock(pbft_block1); // Test pbft chain
  EXPECT_EQ(node2->getPbftChainSize(), 2);
  node3->setPbftBlock(pbft_block1); // Test pbft chain
  EXPECT_EQ(node3->getPbftChainSize(), 2);

  // generate pbft schedule block sample
  blk_hash_t prev_pivot(1);
  uint64_t timestamp2 = 333333;
  vec_blk_t blks { blk_hash_t(123), blk_hash_t(456), blk_hash_t(789) };
  std::vector<std::vector<uint>> modes{
      {0, 1, 2, 0, 1, 2}, {1, 1, 1, 1, 1}, {0, 0, 0}};
  TrxSchedule schedule(blks, modes);
  ScheduleBlock schedule_blk(prev_pivot, timestamp2, schedule);

  PbftBlock pbft_block2(blk_hash_t(2));
  pbft_block2.setScheduleBlock(schedule_blk);

  node1->pushPbftBlockIntoQueue(pbft_block1);
  EXPECT_EQ(node1->getPbftQueueSize(), 2);
  node1->setPbftBlock(pbft_block2); // Test pbft chain
  EXPECT_EQ(node1->getPbftChainSize(), 3);

  nw1->onNewPbftBlock(pbft_block2);

  current_pbft_queue_size = 2;
  for (int i = 0; i < 300; i++) {
    // test timeout is 30 seconds
    if (node2->getPbftQueueSize() == current_pbft_queue_size &&
        node3->getPbftQueueSize() == current_pbft_queue_size) {
      break;
    }
    taraxa::thisThreadSleepForMilliSeconds(100);
  }
  EXPECT_EQ(node2->getPbftQueueSize(), current_pbft_queue_size);
  EXPECT_EQ(node3->getPbftQueueSize(), current_pbft_queue_size);
  node2->setPbftBlock(pbft_block2); // Test pbft chain
  EXPECT_EQ(node2->getPbftChainSize(), 3);
  node3->setPbftBlock(pbft_block2); // Test pbft chain
  EXPECT_EQ(node3->getPbftChainSize(), 3);

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

/*
// TODO: not done
TEST(PbftChain, simulate_pbft_execute_round) {
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
  node1->start();
  node2->start();
  node3->start();

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

  // Max Taraxa coins 2^53 - 1, make sure will get sortition
  bal_t new_balance = 9007199254740991;
  // setup account1 for node1
  dev::KeyPair key_pair1 = dev::KeyPair::create();
  addr_t account_address1 = dev::toAddress(key_pair1.pub());
  node1->setBalance(account_address1, new_balance);
  // setup account2 for node2
  dev::KeyPair key_pair2 = dev::KeyPair::create();
  addr_t account_address2 = dev::toAddress(key_pair2.pub());
  node2->setBalance(account_address2, new_balance);
  // setup account3 for node3
  dev::KeyPair key_pair3 = dev::KeyPair::create();
  addr_t account_address3 = dev::toAddress(key_pair3.pub());
  node3->setBalance(account_address3, new_balance);

  // start time
  auto now = std::chrono::system_clock::now();

  // generate vote and pbft pivot block sample
  blk_hash_t pivot_hash(1);
  blk_hash_t prev_pivot_blk(0);
  blk_hash_t prev_res_blk(0);
  blk_hash_t dag_blk(1111111);
  uint64_t epoch = 1;
  uint64_t timestamp1 = 123456;
  addr_t beneficiary = dev::toAddress(key_pairs[0].pub());
  size_t period = 1;
  size_t step = 1;
  std::string message = pivot_hash.toString() +
                        std::to_string(pivot_block_type) +
                        std::to_string(period) +
                        std::to_string(step);
  sig_t sig1 = dev::sign(key_pair1.secret(), dev::sha3(message));
  // node1 should speak
  bool node1_should_speak = shouldSpeak(pivot_hash, pivot_block_type, period, step);
  EXPECT_TRUE(node1_should_speak);

  Vote vote1(key_pair1.pub(), sig1, pivot_hash, pivot_block_type, period, step);
  PivotBlock pivot_block(pivot_hash, prev_pivot_blk, prev_res_blk, dag_blk,
                         epoch, timestamp1, beneficiary, sig1);
  PbftBlock pbft_block1(pivot_hash);
  pbft_block1.setPivotBlock(pivot_block);
  pbft_block1.setBlockType(pivot_block_type);

  // node1 put vote1 into vote queue
  node1->placeVote(vote1);
  EXPECT_EQ(node1->getVoteQueueSize(), 1);

  // node1 broadcast vote1 to node2 and node3
  nw1->onNewPbftVote(vote1);
  for (int i = 0; i < 300; ++i) {
    // test timeout is 30 seconds
    if (node2->getVoteQueueSize() == 1 &&
        node3->getVoteQueueSize() == 1) {
      break;
    }
    taraxa::thisThreadSleepForMilliSeconds(100);
  }
  EXPECT_EQ(node2->getVoteQueueSize(), 1);
  EXPECT_EQ(node3->getVoteQueueSize(), 1);

  //node1 proposal pbft pivot block to node2 and node3
  nw1->onNewPbftBlock(pbft_block1);

  for (int i = 0; i < 300; i++) {
    // test timeout is 30 seconds
    if (node2->getPbftQueueSize() == 1 &&
        node3->getPbftQueueSize() == 1) {
      break;
    }
    taraxa::thisThreadSleepForMilliSeconds(100);
  }
  EXPECT_EQ(node1->getPbftQueueSize(), 0);
  EXPECT_EQ(node2->getPbftQueueSize(), 1);
  EXPECT_EQ(node3->getPbftQueueSize(), 1);

  //node2 should speak and generate soft vote
  std::pair<PbftBlock, bool> leader =
  bool node2_should_speak = shouldSpeak(pivot_block)




  //node1->setPbftBlock(pbft_block1);
  //size_t node1_pbft_chain_size = node1->getPbftChainSize();
  //EXPECT_EQ(node1_pbft_chain_size, 2);




  nw1->onNewPbftBlock(pbft_block1);

  int current_pbft_chain_size = 2;
  for (int i = 0; i < 300; i++) {
    // test timeout is 30 seconds
    if (node2->getPbftChainSize() == current_pbft_chain_size &&
        node3->getPbftChainSize() == current_pbft_chain_size) {
      break;
    }
    taraxa::thisThreadSleepForMilliSeconds(100);
  }
  size_t node2_pbft_chain_size = node2->getPbftChainSize();
  size_t node3_pbft_chain_size = node3->getPbftChainSize();
  EXPECT_EQ(node2_pbft_chain_size, current_pbft_chain_size);
  EXPECT_EQ(node3_pbft_chain_size, current_pbft_chain_size);

  // generate pbft schedule block sample
  blk_hash_t schedule_hash(2);
  blk_hash_t prev_pivot(1);
  uint64_t timestamp2 = 333333;
  sig_t sig2(444444);
  vec_blk_t blks { blk_hash_t(123), blk_hash_t(456), blk_hash_t(789) };
  std::vector<std::vector<uint>> modes{
      {0, 1, 2, 0, 1, 2}, {1, 1, 1, 1, 1}, {0, 0, 0}};
  TrxSchedule schedule(blks, modes);
  ScheduleBlock schedule_blk(schedule_hash, prev_pivot, timestamp2, sig2,
                             schedule);

  PbftBlock pbft_block2;
  pbft_block2.setScheduleBlock(schedule_blk);
  pbft_block2.setBlockType(schedule_block_type);

  node1->setPbftBlock(pbft_block2);
  node1_pbft_chain_size = node1->getPbftChainSize();
  EXPECT_EQ(node1_pbft_chain_size, 3);

  nw1->onNewPbftBlock(pbft_block2);

  current_pbft_chain_size = 3;
  for (int i = 0; i < 300; i++) {
    // test timeout is 30 seconds
    if (node2->getPbftChainSize() == current_pbft_chain_size &&
        node3->getPbftChainSize() == current_pbft_chain_size) {
      break;
    }
    taraxa::thisThreadSleepForMilliSeconds(100);
  }
  node2_pbft_chain_size = node2->getPbftChainSize();
  node3_pbft_chain_size = node3->getPbftChainSize();
  EXPECT_EQ(node2_pbft_chain_size, current_pbft_chain_size);
  EXPECT_EQ(node3_pbft_chain_size, current_pbft_chain_size);

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
*/

}  // namespace taraxa

int main(int argc, char** argv) {
  dev::LoggingOptions logOptions;
  logOptions.verbosity = dev::VerbosityError;
  logOptions.includeChannels.push_back("PBFT_MGR");
  logOptions.includeChannels.push_back("NETWORK");
  logOptions.includeChannels.push_back("TARCAP");
  dev::setupLogging(logOptions);
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
