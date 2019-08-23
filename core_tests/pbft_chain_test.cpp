/*
 * @Copyright: Taraxa.io
 * @Author: Chia-Chun Lin
 * @Date: 2019-04-08 15:59:01
 * @Last Modified by: Qi Gao
 * @Last Modified time: 2019-08-15
 */

#include "pbft_chain.hpp"
#include <gtest/gtest.h>
#include <atomic>
#include <boost/thread.hpp>
#include <iostream>
#include <vector>
#include "full_node.hpp"
#include "libdevcore/DBFactory.h"
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
  uint64_t period = 1;
  addr_t beneficiary(10);
  PivotBlock pivot_block1(prev_pivot_blk, prev_res_blk, dag_blk, period,
                          beneficiary);

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
  vec_blk_t blks{blk_hash_t(123), blk_hash_t(456), blk_hash_t(789)};
  std::vector<std::vector<uint>> modes{
      {0, 1, 2, 0, 1, 2}, {1, 1, 1, 1, 1}, {0, 0, 0}};
  TrxSchedule schedule(blks, modes);
  ScheduleBlock schedule_blk1(prev_pivot, schedule);

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

  // Compare concurrent schedule content
  EXPECT_EQ(ss1.str(), ss2.str());
}

TEST(PbftChain, pbft_db_test) {
  boost::asio::io_context context;
  auto node(std::make_shared<taraxa::FullNode>(
      context, std::string("./core_tests/conf/conf_taraxa1.json")));
  std::shared_ptr<SimpleDBFace> db_pbftchain = node->getPbftChainDB();
  std::shared_ptr<PbftChain> pbft_chain = node->getPbftChain();
  std::string pbft_genesis_from_db =
      db_pbftchain->get(pbft_chain->getGenesisHash().toString());
  EXPECT_FALSE(pbft_genesis_from_db.empty());

  // generate pbft pivot block sample
  blk_hash_t prev_pivot_hash(0);
  blk_hash_t prev_blk_hash(0);
  blk_hash_t dag_blk_hash(78);
  uint64_t pbft_chain_period = 1;
  addr_t beneficiary(10);
  PivotBlock pivot_block(prev_pivot_hash, prev_blk_hash, dag_blk_hash,
                         pbft_chain_period, beneficiary);
  PbftBlock pbft_block1(blk_hash_t(1));
  pbft_block1.setPivotBlock(pivot_block);
  // setup timestamp for pbft block
  pbft_block1.setTimestamp(std::time(nullptr));
  // sign the pbft block
  pbft_block1.setSignature(node->signMessage(pbft_block1.getJsonStr()));

  // put into pbft chain and store into DB
  node->setPbftBlock(pbft_block1);
  EXPECT_EQ(node->getPbftChainSize(), 2);

  std::string pbft_block_from_db =
      db_pbftchain->get(pbft_block1.getBlockHash().toString());
  PbftBlock pbft_block2(pbft_block_from_db);

  std::stringstream ss1, ss2;
  ss1 << pbft_block1;
  ss2 << pbft_block2;
  EXPECT_EQ(ss1.str(), ss2.str());

  // generate pbft schedule block sample
  blk_hash_t prev_pivot(1);
  TrxSchedule schedule;
  ScheduleBlock schedule_blk(prev_pivot, schedule);

  PbftBlock pbft_block3(blk_hash_t(2));
  pbft_block3.setScheduleBlock(schedule_blk);
  // setup timestamp for pbft block
  pbft_block3.setTimestamp(std::time(nullptr));
  // sign the pbft block
  pbft_block3.setSignature(node->signMessage(pbft_block3.getJsonStr()));

  // put into pbft chain and store into DB
  node->setPbftBlock(pbft_block3);
  EXPECT_EQ(node->getPbftChainSize(), 3);

  pbft_block_from_db = db_pbftchain->get(pbft_block3.getBlockHash().toString());
  PbftBlock pbft_block4(pbft_block_from_db);

  std::stringstream ss3, ss4;
  ss3 << pbft_block3;
  ss4 << pbft_block4;
  EXPECT_EQ(ss3.str(), ss4.str());

  // check pbft genesis update in DB
  pbft_genesis_from_db =
      db_pbftchain->get(pbft_chain->getGenesisHash().toString());
  EXPECT_EQ(pbft_genesis_from_db, pbft_chain->getJsonStr());
}

TEST(PbftChain, block_broadcast) {
  boost::asio::io_context context1;
  auto node1(std::make_shared<taraxa::FullNode>(
      context1, std::string("./core_tests/conf/conf_taraxa1.json")));
  boost::asio::io_context context2;
  auto node2(std::make_shared<taraxa::FullNode>(
      context2, std::string("./core_tests/conf/conf_taraxa2.json")));
  boost::asio::io_context context3;
  auto node3(std::make_shared<taraxa::FullNode>(
      context3, std::string("./core_tests/conf/conf_taraxa3.json")));

  std::shared_ptr<PbftManager> pbft_mgr1 = node1->getPbftManager();
  std::shared_ptr<PbftManager> pbft_mgr2 = node2->getPbftManager();
  std::shared_ptr<PbftManager> pbft_mgr3 = node3->getPbftManager();

  node1->setDebug(true);
  node2->setDebug(true);
  node3->setDebug(true);
  node1->start(true);  // boot_node
  node2->start(false);
  node3->start(false);

  pbft_mgr1->stop();
  pbft_mgr2->stop();
  pbft_mgr3->stop();

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
  addr_t beneficiary(10);
  PivotBlock pivot_block(prev_pivot_blk, prev_res_blk, dag_blk, epoch,
                         beneficiary);
  PbftBlock pbft_block1(blk_hash_t(1));
  pbft_block1.setPivotBlock(pivot_block);
  // setup timestamp for pbft block
  pbft_block1.setTimestamp(std::time(nullptr));
  // sign the pbft block
  pbft_block1.setSignature(node1->signMessage(pbft_block1.getJsonStr()));

  node1->pushPbftBlockIntoQueue(pbft_block1);
  EXPECT_EQ(node1->getPbftUnverifiedQueueSize(), 1);
  node1->setPbftBlock(pbft_block1);  // Test pbft chain
  ASSERT_EQ(node1->getPbftChainSize(), 2);

  nw1->onNewPbftBlock(pbft_block1);

  int current_pbft_queue_size = 1;
  for (int i = 0; i < 300; i++) {
    // test timeout is 30 seconds
    if (node2->getPbftUnverifiedQueueSize() == current_pbft_queue_size &&
        node3->getPbftUnverifiedQueueSize() == current_pbft_queue_size) {
      break;
    }
    taraxa::thisThreadSleepForMilliSeconds(100);
  }
  EXPECT_EQ(node2->getPbftUnverifiedQueueSize(), current_pbft_queue_size);
  EXPECT_EQ(node3->getPbftUnverifiedQueueSize(), current_pbft_queue_size);
  node2->setPbftBlock(pbft_block1);  // Test pbft chain
  ASSERT_EQ(node2->getPbftChainSize(), 2);
  node3->setPbftBlock(pbft_block1);  // Test pbft chain
  ASSERT_EQ(node3->getPbftChainSize(), 2);

  // generate pbft schedule block sample
  blk_hash_t prev_pivot(1);
  TrxSchedule schedule;
  ScheduleBlock schedule_blk(prev_pivot, schedule);

  PbftBlock pbft_block2(blk_hash_t(2));
  pbft_block2.setScheduleBlock(schedule_blk);
  // setup timestamp for pbft block
  pbft_block2.setTimestamp(std::time(nullptr));
  // sign the pbft block
  pbft_block2.setSignature(node1->signMessage(pbft_block2.getJsonStr()));

  node1->pushPbftBlockIntoQueue(pbft_block2);
  EXPECT_EQ(node1->getPbftUnverifiedQueueSize(), 2);
  node1->setPbftBlock(pbft_block2);  // Test pbft chain
  ASSERT_EQ(node1->getPbftChainSize(), 3);

  nw1->onNewPbftBlock(pbft_block2);

  current_pbft_queue_size = 2;
  for (int i = 0; i < 300; i++) {
    // test timeout is 30 seconds
    if (node2->getPbftUnverifiedQueueSize() == current_pbft_queue_size &&
        node3->getPbftUnverifiedQueueSize() == current_pbft_queue_size) {
      break;
    }
    taraxa::thisThreadSleepForMilliSeconds(100);
  }
  EXPECT_EQ(node2->getPbftUnverifiedQueueSize(), current_pbft_queue_size);
  EXPECT_EQ(node3->getPbftUnverifiedQueueSize(), current_pbft_queue_size);
  node2->setPbftBlock(pbft_block2);  // Test pbft chain
  ASSERT_EQ(node2->getPbftChainSize(), 3);
  node3->setPbftBlock(pbft_block2);  // Test pbft chain
  ASSERT_EQ(node3->getPbftChainSize(), 3);

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

TEST(PbftChain, get_dag_block_hash) {
  boost::asio::io_context context;
  auto node(std::make_shared<taraxa::FullNode>(
      context, std::string("./core_tests/conf/conf_taraxa1.json")));

  std::shared_ptr<PbftChain> pbft_chain = node->getPbftChain();
  std::pair<blk_hash_t, bool> dag_genesis_hash = pbft_chain->getDagBlockHash(0);
  ASSERT_TRUE(dag_genesis_hash.second);
  ASSERT_EQ(dag_genesis_hash.first,
            node->getConfig().genesis_state.block.getHash());
}

TEST(PbftChain, get_dag_block_height) {
  boost::asio::io_context context;
  auto node(std::make_shared<taraxa::FullNode>(
      context, std::string("./core_tests/conf/conf_taraxa1.json")));

  std::shared_ptr<PbftChain> pbft_chain = node->getPbftChain();
  std::pair<uint64_t, bool> dag_genesis_height = pbft_chain->getDagBlockHeight(
      node->getConfig().genesis_state.block.getHash());
  ASSERT_TRUE(dag_genesis_height.second);
  ASSERT_EQ(dag_genesis_height.first, 0);
}

}  // namespace taraxa

int main(int argc, char** argv) {
  TaraxaStackTrace st;
  dev::LoggingOptions logOptions;
  logOptions.verbosity = dev::VerbosityError;
  logOptions.includeChannels.push_back("PBFT_CHAIN");
  logOptions.includeChannels.push_back("PBFT_MGR");
  logOptions.includeChannels.push_back("NETWORK");
  logOptions.includeChannels.push_back("TARCAP");
  dev::setupLogging(logOptions);
  // use the in-memory db so test will not affect other each other through
  // persistent storage
  dev::db::setDatabaseKind(dev::db::DatabaseKind::MemoryDB);
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
