/*
 * @Copyright: Taraxa.io
 * @Author: Chia-Chun Lin
 * @Date: 2019-04-08 15:59:01
 * @Last Modified by: Qi Gao
 * @Last Modified time: 2019-05-01
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
  blk_hash_t prev_blk(12);
  blk_hash_t prev_pivot_blk(34);
  blk_hash_t prev_res_blk(56);
  blk_hash_t dag_blk(78);
  uint64_t epoch = 1;
  uint64_t timestamp = 123456;
  addr_t beneficiary(10);
  sig_t sig(1212121212);
  PivotBlock pivot_block1(prev_blk, prev_pivot_blk, prev_res_blk, dag_blk, epoch,
      timestamp, beneficiary, sig);

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

TEST(PivotBlock, pivot_block_broadcast) {
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

  // generate pbft pivot block sample
  blk_hash_t prev_blk(12);
  blk_hash_t prev_pivot_blk(34);
  blk_hash_t prev_res_blk(56);
  blk_hash_t dag_blk(78);
  uint64_t epoch = 1;
  uint64_t timestamp = 123456;
  addr_t beneficiary(10);
  sig_t sig(1212121212);
  PivotBlock pivot_block(prev_blk, prev_pivot_blk, prev_res_blk, dag_blk, epoch,
                          timestamp, beneficiary, sig);
  PbftBlock pbft_block;
  pbft_block.setPivotBlock(pivot_block);
  pbft_block.setBlockType(pivot_block_type);

  node1->setPbftBlock(pbft_block);
  size_t node1_pbft_chain_size = node1->getPbftChainSize();
  EXPECT_EQ(node1_pbft_chain_size, 2);

  nw1->onNewPbftBlock(pbft_block);

  work1.reset();
  work2.reset();
  work3.reset();
  node1->stop();
  node2->stop();
  node3->stop();
  t1.join();
  t2.join();
  t3.join();

  size_t node2_pbft_chain_size = node2->getPbftChainSize();
  size_t node3_pbft_chain_size = node3->getPbftChainSize();
  EXPECT_EQ(node2_pbft_chain_size, 2);
  EXPECT_EQ(node3_pbft_chain_size, 2);
}

TEST(PbftManager, DISABLED_create_pbft_manager) {
  PbftManager pbft_mgr;
  pbft_mgr.start();
  thisThreadSleepForSeconds(1);
  EXPECT_TRUE(pbft_mgr.isActive());
  pbft_mgr.stop();
  EXPECT_FALSE(pbft_mgr.isActive());
}

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
