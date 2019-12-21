#include "pbft_chain.hpp"

#include <gtest/gtest.h>
#include <libdevcore/DBFactory.h>
#include <libdevcore/Log.h>

#include <atomic>
#include <boost/thread.hpp>
#include <iostream>
#include <vector>

#include "full_node.hpp"
#include "network.hpp"
#include "pbft_manager.hpp"
#include "util.hpp"
#include "util/constants.hpp"

namespace taraxa {
using core_tests::util::constants::TEST_TX_GAS_LIMIT;

struct PbftChainTest : core_tests::util::DBUsingTest<> {};

TEST_F(PbftChainTest, serialize_deserialize_trx_schedule) {
  vec_blk_t blks{blk_hash_t(123), blk_hash_t(456), blk_hash_t(32443)};
  std::vector<vec_trx_t> trxs_list{
      {trx_hash_t(1), trx_hash_t(2), trx_hash_t(3)},
      {},
      {trx_hash_t(4), trx_hash_t(5)}};
  std::vector<std::vector<std::pair<trx_hash_t, uint>>> trxs_mode;
  for (int i = 0; i < trxs_list.size(); i++) {
    std::vector<std::pair<trx_hash_t, uint>> one_blk_trxs_mode;
    for (int j = 0; j < trxs_list[i].size(); j++) {
      one_blk_trxs_mode.emplace_back(std::make_pair(trxs_list[i][j], 1));
    }
    trxs_mode.emplace_back(one_blk_trxs_mode);
  }
  EXPECT_EQ(trxs_mode.size(), blks.size());
  TrxSchedule sche1(blks, trxs_mode);
  auto rlp = sche1.rlp();
  TrxSchedule sche2(rlp);
  EXPECT_EQ(sche1, sche2);
}

TEST_F(PbftChainTest, serialize_deserialize_pivot_block) {
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

TEST_F(PbftChainTest, serialize_deserialize_schedule_block) {
  blk_hash_t prev_pivot(22);
  vec_blk_t blks{blk_hash_t(123), blk_hash_t(456), blk_hash_t(789)};
  std::vector<vec_trx_t> trxs_list{
      {trx_hash_t(1), trx_hash_t(2), trx_hash_t(3)},
      {},
      {trx_hash_t(4), trx_hash_t(5)}};
  std::vector<std::vector<std::pair<trx_hash_t, uint>>> trxs_mode;
  for (int i = 0; i < trxs_list.size(); i++) {
    std::vector<std::pair<trx_hash_t, uint>> one_blk_trxs_mode;
    for (int j = 0; j < trxs_list[i].size(); j++) {
      one_blk_trxs_mode.emplace_back(std::make_pair(trxs_list[i][j], 1));
    }
    trxs_mode.emplace_back(one_blk_trxs_mode);
  }
  TrxSchedule schedule(blks, trxs_mode);
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

TEST_F(PbftChainTest, pbft_db_test) {
  auto node(taraxa::FullNode::make(
      std::string("./core_tests/conf/conf_taraxa1.json")));
  node->start(true);  // boot node

  auto db = node->getDB();
  std::shared_ptr<PbftChain> pbft_chain = node->getPbftChain();
  std::string pbft_genesis_from_db =
      db->getPbftBlockGenesis(pbft_chain->getGenesisHash().toString());
  EXPECT_FALSE(pbft_genesis_from_db.empty());

  // generate pbft pivot block sample
  blk_hash_t prev_pivot_hash(0);
  blk_hash_t prev_blk_hash(0);
  blk_hash_t dag_blk_hash(78);
  uint64_t pbft_chain_period = 1;
  addr_t beneficiary(10);
  PivotBlock pivot_block(prev_pivot_hash, prev_blk_hash, dag_blk_hash,
                         pbft_chain_period, beneficiary);
  PbftBlock pbft_block1(blk_hash_t(1), 2);
  pbft_block1.setPivotBlock(pivot_block);
  // setup timestamp for pbft block
  pbft_block1.setTimestamp(std::time(nullptr));
  // sign the pbft block
  pbft_block1.setSignature(node->signMessage(pbft_block1.getJsonStr(false)));

  // put into pbft chain and store into DB
  bool push_block = pbft_chain->pushPbftPivotBlock(pbft_block1);
  EXPECT_TRUE(push_block);
  EXPECT_EQ(node->getPbftChainSize(), 2);

  auto pbft_block2 = db->getPbftBlock(pbft_block1.getBlockHash());

  std::stringstream ss1, ss2;
  ss1 << pbft_block1;
  ss2 << *pbft_block2;
  EXPECT_EQ(ss1.str(), ss2.str());

  // generate pbft schedule block sample
  blk_hash_t prev_pivot(1);
  TrxSchedule schedule;
  ScheduleBlock schedule_blk(prev_pivot, schedule);

  PbftBlock pbft_block3(blk_hash_t(2), 3);
  pbft_block3.setScheduleBlock(schedule_blk);
  // setup timestamp for pbft block
  pbft_block3.setTimestamp(std::time(nullptr));
  // sign the pbft block
  pbft_block3.setSignature(node->signMessage(pbft_block3.getJsonStr(false)));

  // put into pbft chain and store into DB
  push_block = pbft_chain->pushPbftScheduleBlock(pbft_block3);
  EXPECT_TRUE(push_block);
  EXPECT_EQ(node->getPbftChainSize(), 3);

  auto pbft_block4 = db->getPbftBlock(pbft_block3.getBlockHash());

  std::stringstream ss3, ss4;
  ss3 << pbft_block3;
  ss4 << *pbft_block4;
  EXPECT_EQ(ss3.str(), ss4.str());

  // check pbft genesis update in DB
  pbft_genesis_from_db =
      db->getPbftBlockGenesis(pbft_chain->getGenesisHash().toString());
  EXPECT_EQ(pbft_genesis_from_db, pbft_chain->getJsonStr());

  db = nullptr;
}

TEST_F(PbftChainTest, block_broadcast) {
  auto node1(taraxa::FullNode::make(
      std::string("./core_tests/conf/conf_taraxa1.json")));
  auto node2(taraxa::FullNode::make(
      std::string("./core_tests/conf/conf_taraxa2.json")));
  auto node3(taraxa::FullNode::make(
      std::string("./core_tests/conf/conf_taraxa3.json")));

  std::shared_ptr<PbftManager> pbft_mgr1 = node1->getPbftManager();
  std::shared_ptr<PbftManager> pbft_mgr2 = node2->getPbftManager();
  std::shared_ptr<PbftManager> pbft_mgr3 = node3->getPbftManager();

  std::shared_ptr<PbftChain> pbft_chain1 = node1->getPbftChain();
  std::shared_ptr<PbftChain> pbft_chain2 = node2->getPbftChain();
  std::shared_ptr<PbftChain> pbft_chain3 = node3->getPbftChain();

  node1->setDebug(true);
  node2->setDebug(true);
  node3->setDebug(true);
  node1->start(true);  // boot_node
  node2->start(false);
  node3->start(false);

  pbft_mgr1->stop();
  pbft_mgr2->stop();
  pbft_mgr3->stop();

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
  uint64_t period = 1;
  addr_t beneficiary(10);
  PivotBlock pivot_block(prev_pivot_blk, prev_res_blk, dag_blk, period,
                         beneficiary);
  PbftBlock pbft_block1(blk_hash_t(1), 2);
  pbft_block1.setPivotBlock(pivot_block);
  // setup timestamp for pbft block
  pbft_block1.setTimestamp(std::time(nullptr));
  // sign the pbft block
  pbft_block1.setSignature(node1->signMessage(pbft_block1.getJsonStr(false)));

  node1->pushUnverifiedPbftBlock(pbft_block1);
  std::pair<PbftBlock, bool> block1_from_node1 =
      pbft_chain1->getUnverifiedPbftBlock(pbft_block1.getBlockHash());
  ASSERT_TRUE(block1_from_node1.second);
  EXPECT_EQ(block1_from_node1.first.getJsonStr(), pbft_block1.getJsonStr());
  // node1 put block1 into pbft chain and store into DB
  bool push_block = pbft_chain1->pushPbftPivotBlock(pbft_block1);
  EXPECT_TRUE(push_block);
  EXPECT_EQ(node1->getPbftChainSize(), 2);
  // node1 cleanup block1 in PBFT unverified blocks table
  pbft_chain1->cleanupUnverifiedPbftBlocks(pbft_block1);
  bool find_erased_block =
      pbft_chain1->findUnverifiedPbftBlock(pbft_block1.getBlockHash());
  ASSERT_FALSE(find_erased_block);

  nw1->onNewPbftBlock(pbft_block1);

  std::pair<PbftBlock, bool> block1_from_node2;
  std::pair<PbftBlock, bool> block1_from_node3;
  for (int i = 0; i < 300; i++) {
    // test timeout is 30 seconds
    block1_from_node2 =
        pbft_chain2->getUnverifiedPbftBlock(pbft_block1.getBlockHash());
    block1_from_node3 =
        pbft_chain3->getUnverifiedPbftBlock(pbft_block1.getBlockHash());
    if (block1_from_node2.second && block1_from_node3.second) {
      // Both node2 and node3 get the pbft_block1
      break;
    }
    taraxa::thisThreadSleepForMilliSeconds(100);
  }
  ASSERT_TRUE(block1_from_node2.second);
  ASSERT_TRUE(block1_from_node3.second);
  EXPECT_EQ(block1_from_node2.first.getJsonStr(), pbft_block1.getJsonStr());
  EXPECT_EQ(block1_from_node3.first.getJsonStr(), pbft_block1.getJsonStr());
  // node2 put block1 into pbft chain and store into DB
  push_block = pbft_chain2->pushPbftPivotBlock(pbft_block1);
  EXPECT_TRUE(push_block);
  EXPECT_EQ(node2->getPbftChainSize(), 2);
  // node2 cleanup block1 in PBFT unverified blocks table
  pbft_chain2->cleanupUnverifiedPbftBlocks(pbft_block1);
  find_erased_block =
      pbft_chain2->findUnverifiedPbftBlock(pbft_block1.getBlockHash());
  ASSERT_FALSE(find_erased_block);
  // node3 put block1 into pbft chain and store into DB
  push_block = pbft_chain3->pushPbftPivotBlock(pbft_block1);
  EXPECT_TRUE(push_block);
  EXPECT_EQ(node3->getPbftChainSize(), 2);
  // node3 cleanup block1 in PBFT unverified blocks table
  pbft_chain3->cleanupUnverifiedPbftBlocks(pbft_block1);
  find_erased_block =
      pbft_chain3->findUnverifiedPbftBlock(pbft_block1.getBlockHash());
  ASSERT_FALSE(find_erased_block);

  // generate pbft schedule block sample
  blk_hash_t prev_pivot(1);
  TrxSchedule schedule;
  ScheduleBlock schedule_blk(prev_pivot, schedule);
  PbftBlock pbft_block2(blk_hash_t(2), 3);
  pbft_block2.setScheduleBlock(schedule_blk);
  // setup timestamp for pbft block
  pbft_block2.setTimestamp(std::time(nullptr));
  // sign the pbft block
  pbft_block2.setSignature(node1->signMessage(pbft_block2.getJsonStr(false)));

  node1->pushUnverifiedPbftBlock(pbft_block2);
  std::pair<PbftBlock, bool> block2_from_node1 =
      pbft_chain1->getUnverifiedPbftBlock(pbft_block2.getBlockHash());
  ASSERT_TRUE(block2_from_node1.second);
  EXPECT_EQ(block2_from_node1.first.getJsonStr(), pbft_block2.getJsonStr());
  // node1 put block2 into pbft chain and store into DB
  push_block = pbft_chain1->pushPbftScheduleBlock(pbft_block2);
  EXPECT_TRUE(push_block);
  EXPECT_EQ(node1->getPbftChainSize(), 3);
  // node1 cleanup block2 in PBFT unverified blocks table
  pbft_chain1->cleanupUnverifiedPbftBlocks(pbft_block2);
  find_erased_block =
      pbft_chain1->findUnverifiedPbftBlock(pbft_block2.getBlockHash());
  ASSERT_FALSE(find_erased_block);

  nw1->onNewPbftBlock(pbft_block2);

  std::pair<PbftBlock, bool> block2_from_node2;
  std::pair<PbftBlock, bool> block2_from_node3;
  for (int i = 0; i < 300; i++) {
    // test timeout is 30 seconds
    block2_from_node2 =
        pbft_chain2->getUnverifiedPbftBlock(pbft_block2.getBlockHash());
    block2_from_node3 =
        pbft_chain3->getUnverifiedPbftBlock(pbft_block2.getBlockHash());
    if (block2_from_node2.second && block2_from_node3.second) {
      // Both node2 and node3 get pbft_block2
      break;
    }
    taraxa::thisThreadSleepForMilliSeconds(100);
  }
  ASSERT_TRUE(block2_from_node2.second);
  ASSERT_TRUE(block2_from_node3.second);
  EXPECT_EQ(block2_from_node2.first.getJsonStr(), pbft_block2.getJsonStr());
  EXPECT_EQ(block2_from_node3.first.getJsonStr(), pbft_block2.getJsonStr());
  // node2 put block2 into pbft chain and store into DB
  push_block = pbft_chain2->pushPbftScheduleBlock(pbft_block2);
  EXPECT_TRUE(push_block);
  EXPECT_EQ(node2->getPbftChainSize(), 3);
  // node2 cleanup block2 in PBFT unverified blocks table
  pbft_chain2->cleanupUnverifiedPbftBlocks(pbft_block2);
  find_erased_block =
      pbft_chain2->findUnverifiedPbftBlock(pbft_block2.getBlockHash());
  ASSERT_FALSE(find_erased_block);
  // node3 put block2 into pbft chain and store into DB
  push_block = pbft_chain3->pushPbftScheduleBlock(pbft_block2);
  EXPECT_TRUE(push_block);
  EXPECT_EQ(node3->getPbftChainSize(), 3);
  // node3 cleanup block2 in PBFT unverified blocks table
  pbft_chain3->cleanupUnverifiedPbftBlocks(pbft_block2);
  find_erased_block =
      pbft_chain3->findUnverifiedPbftBlock(pbft_block2.getBlockHash());
  ASSERT_FALSE(find_erased_block);
}

TEST_F(PbftChainTest, get_dag_block_hash) {
  auto node(taraxa::FullNode::make(
      std::string("./core_tests/conf/conf_taraxa1.json")));
  node->setDebug(true);
  node->start(true);  // boot_node

  std::shared_ptr<PbftChain> pbft_chain = node->getPbftChain();
  std::pair<blk_hash_t, bool> dag_genesis_hash = pbft_chain->getDagBlockHash(1);
  ASSERT_TRUE(dag_genesis_hash.second);
  ASSERT_EQ(dag_genesis_hash.first,
            node->getConfig().dag_genesis_block.getHash());

  // create a transaction
  auto nonce = val_t(0);
  auto coins_value = val_t(100);
  auto gas_price = val_t(2);
  auto receiver = addr_t("973ecb1c08c8eb5a7eaa0d3fd3aab7924f2838b0");
  auto data = bytes();
  auto g_secret = dev::Secret(
      "3800b2875669d9b2053c1aff9224ecfdc411423aac5b5a73d7a45ced1c3b9dcd",
      dev::Secret::ConstructFromStringType::FromHex);
  Transaction trx_master_boot_node_to_receiver(nonce, coins_value, gas_price,
                                               TEST_TX_GAS_LIMIT, receiver,
                                               data, g_secret);
  node->insertTransaction(trx_master_boot_node_to_receiver);

  for (int i = 0; i < 1000; i++) {
    // test timeout is 100 seconds
    if (node->getNumProposedBlocks() == 1) {
      break;
    }
    taraxa::thisThreadSleepForMilliSeconds(100);
  }
  EXPECT_EQ(node->getNumProposedBlocks(), 1);

  // Vote DAG block
  for (int i = 0; i < 600; i++) {
    // test timeout is 60 seconds
    if (pbft_chain->getPbftChainSize() == 3) {
      break;
    }
    taraxa::thisThreadSleepForMilliSeconds(100);
  }
  EXPECT_EQ(pbft_chain->getPbftChainSize(), 3);

  vector<blk_hash_t> dag_blocks_hash = node->getLinearizedDagBlocks();
  EXPECT_EQ(dag_blocks_hash.size(), 2);

  auto dag_max_height = node->getDagBlockMaxHeight();
  ASSERT_EQ(dag_max_height, 2);

  std::pair<blk_hash_t, bool> last_dag_block_hash =
      pbft_chain->getDagBlockHash(2);
  ASSERT_TRUE(last_dag_block_hash.second);
  std::pair<uint64_t, bool> last_dag_block_height =
      pbft_chain->getDagBlockHeight(last_dag_block_hash.first);
  ASSERT_TRUE(last_dag_block_height.second);
  ASSERT_EQ(last_dag_block_height.first, 2);
}

TEST_F(PbftChainTest, get_dag_block_height) {
  auto node(taraxa::FullNode::make(
      std::string("./core_tests/conf/conf_taraxa1.json")));
  node->setDebug(true);
  node->start(true);  // boot_node

  std::shared_ptr<PbftChain> pbft_chain = node->getPbftChain();
  std::pair<uint64_t, bool> dag_genesis_height = pbft_chain->getDagBlockHeight(
      node->getConfig().dag_genesis_block.getHash());
  ASSERT_TRUE(dag_genesis_height.second);
  ASSERT_EQ(dag_genesis_height.first, 1);
}

}  // namespace taraxa

int main(int argc, char** argv) {
  TaraxaStackTrace st;
  dev::LoggingOptions logOptions;
  logOptions.verbosity = dev::VerbosityError;
  logOptions.includeChannels.push_back("PBFT_CHAIN");
  // logOptions.includeChannels.push_back("PBFT_MGR");
  // logOptions.includeChannels.push_back("NETWORK");
  // logOptions.includeChannels.push_back("TARCAP");
  dev::setupLogging(logOptions);
  dev::db::setDatabaseKind(dev::db::DatabaseKind::RocksDB);
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
