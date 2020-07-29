#include "pbft_chain.hpp"

#include <gtest/gtest.h>
#include <libdevcore/Log.h>

#include <atomic>
#include <boost/thread.hpp>
#include <iostream>
#include <vector>

#include "full_node.hpp"
#include "network.hpp"
#include "pbft_manager.hpp"
#include "static_init.hpp"
#include "util.hpp"
#include "util/constants.hpp"

namespace taraxa {
using core_tests::util::constants::TEST_TX_GAS_LIMIT;
using namespace core_tests::util;

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

TEST_F(PbftChainTest, serialize_desiriablize_pbft_block) {
  auto node(taraxa::FullNode::make(FullNodeConfig(conf_file[0]), true));
  node->start(false);
  // Generate PBFT block sample
  blk_hash_t prev_block_hash(12345);
  blk_hash_t dag_block_hash_as_pivot(45678);
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
  EXPECT_EQ(trxs_mode.size(), blks.size());
  TrxSchedule schedule(blks, trxs_mode);
  uint64_t period = 1;
  addr_t beneficiary(98765);
  PbftBlock pbft_block1(prev_block_hash, dag_block_hash_as_pivot, schedule,
                        period, beneficiary, node->getSecretKey());

  auto rlp = pbft_block1.rlp(true);
  PbftBlock pbft_block2(rlp);
  EXPECT_EQ(pbft_block1.getJsonStr(), pbft_block2.getJsonStr());
}

TEST_F(PbftChainTest, pbft_db_test) {
  auto node(taraxa::FullNode::make(std::string(conf_file[0])));
  node->start(true);  // boot node
  auto db = node->getDB();
  std::shared_ptr<PbftChain> pbft_chain = node->getPbftChain();
  blk_hash_t pbft_chain_head_hash = pbft_chain->getHeadHash();
  std::string pbft_head_from_db = db->getPbftHead(pbft_chain_head_hash);
  EXPECT_FALSE(pbft_head_from_db.empty());

  // generate PBFT block sample
  blk_hash_t prev_block_hash(0);
  blk_hash_t dag_blk(123);
  TrxSchedule schedule;
  uint64_t period = 1;
  addr_t beneficiary(987);
  PbftBlock pbft_block1(prev_block_hash, dag_blk, schedule, period, beneficiary,
                        node->getSecretKey());

  // put into pbft chain and store into DB
  auto batch = db->createWriteBatch();
  // Add PBFT block in DB
  db->addPbftBlockToBatch(pbft_block1, batch);
  // Update pbft chain
  pbft_chain->updatePbftChain(pbft_block1.getBlockHash());
  // Update PBFT chain head block
  std::string pbft_chain_head_str = pbft_chain->getJsonStr();
  db->addPbftHeadToBatch(pbft_chain_head_hash, pbft_chain_head_str, batch);
  db->commitWriteBatch(batch);
  int expect_pbft_chain_size = 1;
  EXPECT_EQ(node->getPbftChain()->getPbftChainSize(), expect_pbft_chain_size);

  auto pbft_block2 = db->getPbftBlock(pbft_block1.getBlockHash());
  EXPECT_EQ(pbft_block1.getJsonStr(), pbft_block2->getJsonStr());

  // check pbft genesis update in DB
  pbft_head_from_db = db->getPbftHead(pbft_chain_head_hash);
  EXPECT_EQ(pbft_head_from_db, pbft_chain->getJsonStr());

  db = nullptr;
}

TEST_F(PbftChainTest, block_broadcast) {
  auto node1(taraxa::FullNode::make(conf_file[0]));
  auto node2(taraxa::FullNode::make(conf_file[1]));
  auto node3(taraxa::FullNode::make(conf_file[2]));

  std::shared_ptr<PbftManager> pbft_mgr1 = node1->getPbftManager();
  std::shared_ptr<PbftManager> pbft_mgr2 = node2->getPbftManager();
  std::shared_ptr<PbftManager> pbft_mgr3 = node3->getPbftManager();

  std::shared_ptr<PbftChain> pbft_chain1 = node1->getPbftChain();
  std::shared_ptr<PbftChain> pbft_chain2 = node2->getPbftChain();
  std::shared_ptr<PbftChain> pbft_chain3 = node3->getPbftChain();

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

  // generate first PBFT block sample
  blk_hash_t prev_block_hash(0);
  blk_hash_t dag_blk(123);
  TrxSchedule schedule;
  uint64_t period = 1;
  addr_t beneficiary(987);
  PbftBlock pbft_block(prev_block_hash, dag_blk, schedule, period, beneficiary,
                       node1->getSecretKey());

  node1->getPbftChain()->pushUnverifiedPbftBlock(pbft_block);
  std::pair<PbftBlock, bool> block1_from_node1 =
      pbft_chain1->getUnverifiedPbftBlock(pbft_block.getBlockHash());
  ASSERT_TRUE(block1_from_node1.second);
  EXPECT_EQ(block1_from_node1.first.getJsonStr(), pbft_block.getJsonStr());
  // node1 put block into pbft chain and store into DB
  auto db1 = node1->getDB();
  auto batch = db1->createWriteBatch();
  // Add PBFT block in DB
  db1->addPbftBlockToBatch(pbft_block, batch);
  // Update pbft chain
  pbft_chain1->updatePbftChain(pbft_block.getBlockHash());
  // Update PBFT chain head block
  blk_hash_t pbft_chain_head_hash = pbft_chain1->getHeadHash();
  std::string pbft_chain_head_str = pbft_chain1->getJsonStr();
  db1->addPbftHeadToBatch(pbft_chain_head_hash, pbft_chain_head_str, batch);
  db1->commitWriteBatch(batch);
  int expect_pbft_chain_size = 1;
  EXPECT_EQ(node1->getPbftChain()->getPbftChainSize(), expect_pbft_chain_size);
  // node1 cleanup block1 in PBFT unverified blocks table
  pbft_chain1->cleanupUnverifiedPbftBlocks(pbft_block);
  bool find_erased_block =
      pbft_chain1->findUnverifiedPbftBlock(pbft_block.getBlockHash());
  ASSERT_FALSE(find_erased_block);

  nw1->onNewPbftBlock(pbft_block);

  std::pair<PbftBlock, bool> pbft_block_from_node2;
  std::pair<PbftBlock, bool> pbft_block_from_node3;
  for (int i = 0; i < 300; i++) {
    // test timeout is 30 seconds
    pbft_block_from_node2 =
        pbft_chain2->getUnverifiedPbftBlock(pbft_block.getBlockHash());
    pbft_block_from_node3 =
        pbft_chain3->getUnverifiedPbftBlock(pbft_block.getBlockHash());
    if (pbft_block_from_node2.second && pbft_block_from_node3.second) {
      // Both node2 and node3 get the pbft_block
      break;
    }
    taraxa::thisThreadSleepForMilliSeconds(100);
  }
  ASSERT_TRUE(pbft_block_from_node2.second);
  ASSERT_TRUE(pbft_block_from_node3.second);
  EXPECT_EQ(pbft_block_from_node2.first.getJsonStr(), pbft_block.getJsonStr());
  EXPECT_EQ(pbft_block_from_node3.first.getJsonStr(), pbft_block.getJsonStr());

  // node2 put block into pbft chain and store into DB
  auto db2 = node2->getDB();
  batch = db2->createWriteBatch();
  // Add PBFT block in DB
  db2->addPbftBlockToBatch(pbft_block, batch);
  // Update PBFT chain
  pbft_chain2->updatePbftChain(pbft_block.getBlockHash());
  // Update PBFT chain head block
  pbft_chain_head_hash = pbft_chain2->getHeadHash();
  pbft_chain_head_str = pbft_chain2->getJsonStr();
  db2->addPbftHeadToBatch(pbft_chain_head_hash, pbft_chain_head_str, batch);
  db2->commitWriteBatch(batch);
  EXPECT_EQ(node2->getPbftChain()->getPbftChainSize(), expect_pbft_chain_size);
  // node2 cleanup block1 in PBFT unverified blocks table
  pbft_chain2->cleanupUnverifiedPbftBlocks(pbft_block);
  find_erased_block =
      pbft_chain2->findUnverifiedPbftBlock(pbft_block.getBlockHash());
  ASSERT_FALSE(find_erased_block);
  // node3 put block into pbft chain and store into DB
  auto db3 = node3->getDB();
  batch = db3->createWriteBatch();
  // Add PBFT block in DB
  db3->addPbftBlockToBatch(pbft_block, batch);
  // Update PBFT chain
  pbft_chain3->updatePbftChain(pbft_block.getBlockHash());
  // Update PBFT chain head block
  pbft_chain_head_hash = pbft_chain3->getHeadHash();
  pbft_chain_head_str = pbft_chain3->getJsonStr();
  db3->addPbftHeadToBatch(pbft_chain_head_hash, pbft_chain_head_str, batch);
  db3->commitWriteBatch(batch);
  EXPECT_EQ(node3->getPbftChain()->getPbftChainSize(), expect_pbft_chain_size);
  // node3 cleanup block1 in PBFT unverified blocks table
  pbft_chain3->cleanupUnverifiedPbftBlocks(pbft_block);
  find_erased_block =
      pbft_chain3->findUnverifiedPbftBlock(pbft_block.getBlockHash());
  ASSERT_FALSE(find_erased_block);
}

}  // namespace taraxa

int main(int argc, char** argv) {
  taraxa::static_init();
  dev::LoggingOptions logOptions;
  logOptions.verbosity = dev::VerbosityError;
  logOptions.includeChannels.push_back("PBFT_CHAIN");
  // logOptions.includeChannels.push_back("PBFT_MGR");
  // logOptions.includeChannels.push_back("NETWORK");
  // logOptions.includeChannels.push_back("TARCAP");
  dev::setupLogging(logOptions);
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
