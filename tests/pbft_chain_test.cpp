#include "consensus/pbft_chain.hpp"

#include <gtest/gtest.h>

#include <atomic>
#include <iostream>
#include <vector>

#include "common/static_init.hpp"
#include "consensus/pbft_manager.hpp"
#include "logger/log.hpp"
#include "network/network.hpp"
#include "util_test/util.hpp"

namespace taraxa::core_tests {

struct PbftChainTest : BaseTest {};

TEST_F(PbftChainTest, serialize_desiriablize_pbft_block) {
  auto node_cfgs = make_node_cfgs(1);
  dev::Secret sk(node_cfgs[0].node_secret);
  // Generate PBFT block sample
  blk_hash_t prev_block_hash(12345);
  blk_hash_t dag_block_hash_as_pivot(45678);
  uint64_t period = 1;
  addr_t beneficiary(98765);
  PbftBlock pbft_block1(prev_block_hash, dag_block_hash_as_pivot, period, beneficiary, sk);

  auto rlp = pbft_block1.rlp(true);
  PbftBlock pbft_block2(rlp);
  EXPECT_EQ(pbft_block1.getJsonStr(), pbft_block2.getJsonStr());
}

TEST_F(PbftChainTest, pbft_db_test) {
  auto node_cfgs = make_node_cfgs(1);
  FullNode::Handle node(node_cfgs[0]);
  auto db = node->getDB();
  std::shared_ptr<PbftChain> pbft_chain = node->getPbftChain();
  blk_hash_t pbft_chain_head_hash = pbft_chain->getHeadHash();
  std::string pbft_head_from_db = db->getPbftHead(pbft_chain_head_hash);
  EXPECT_FALSE(pbft_head_from_db.empty());

  // generate PBFT block sample
  blk_hash_t prev_block_hash(0);
  blk_hash_t dag_blk(123);
  uint64_t period = 1;
  addr_t beneficiary(987);
  PbftBlock pbft_block(prev_block_hash, dag_blk, period, beneficiary, node->getSecretKey());

  // put into pbft chain and store into DB
  auto batch = db->createWriteBatch();
  // Add PBFT block in DB
  db->addPbftBlockToBatch(pbft_block, batch);
  // Update PBFT chain
  pbft_chain->updatePbftChain(pbft_block.getBlockHash());
  // Update executed PBFT chain size
  pbft_chain->updateExecutedPbftChainSize();
  // Update PBFT chain head block
  std::string pbft_chain_head_str = pbft_chain->getJsonStr();
  db->addPbftHeadToBatch(pbft_chain_head_hash, pbft_chain_head_str, batch);
  db->commitWriteBatch(batch);
  EXPECT_EQ(pbft_chain->getPbftChainSize(), 1);
  EXPECT_EQ(pbft_chain->getPbftExecutedChainSize(), 1);

  auto pbft_block_from_db = db->getPbftBlock(pbft_block.getBlockHash());
  EXPECT_EQ(pbft_block.getJsonStr(), pbft_block_from_db->getJsonStr());

  // check pbft genesis update in DB
  pbft_head_from_db = db->getPbftHead(pbft_chain_head_hash);
  EXPECT_EQ(pbft_head_from_db, pbft_chain->getJsonStr());
}

TEST_F(PbftChainTest, block_broadcast) {
  auto node_cfgs = make_node_cfgs(3);
  auto nodes = launch_nodes(node_cfgs);
  auto &node1 = nodes[0];
  auto &node2 = nodes[1];
  auto &node3 = nodes[2];

  // Stop PBFT manager and executor to test networking
  node1->getPbftManager()->stop();
  node2->getPbftManager()->stop();
  node3->getPbftManager()->stop();
  node1->getExecutor()->stop();
  node2->getExecutor()->stop();
  node3->getExecutor()->stop();

  std::shared_ptr<PbftChain> pbft_chain1 = node1->getPbftChain();
  std::shared_ptr<PbftChain> pbft_chain2 = node2->getPbftChain();
  std::shared_ptr<PbftChain> pbft_chain3 = node3->getPbftChain();

  std::shared_ptr<Network> nw1 = node1->getNetwork();
  std::shared_ptr<Network> nw2 = node2->getNetwork();
  std::shared_ptr<Network> nw3 = node3->getNetwork();

  const int node_peers = 2;
  for (int i = 0; i < 300; i++) {
    // test timeout is 30 seconds
    if (nw1->getPeerCount() == node_peers && nw2->getPeerCount() == node_peers && nw3->getPeerCount() == node_peers) {
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
  uint64_t period = 1;
  addr_t beneficiary(987);
  auto pbft_block = s_ptr(new PbftBlock(prev_block_hash, dag_blk, period, beneficiary, node1->getSecretKey()));

  node1->getPbftChain()->pushUnverifiedPbftBlock(pbft_block);
  auto block1_from_node1 = pbft_chain1->getUnverifiedPbftBlock(pbft_block->getBlockHash());
  ASSERT_TRUE(block1_from_node1);
  EXPECT_EQ(block1_from_node1->getJsonStr(), pbft_block->getJsonStr());
  // node1 put block into pbft chain and store into DB
  auto db1 = node1->getDB();
  auto batch = db1->createWriteBatch();
  // Add PBFT block in DB
  db1->addPbftBlockToBatch(*pbft_block, batch);
  // Update pbft chain
  pbft_chain1->updatePbftChain(pbft_block->getBlockHash());
  // Update PBFT chain head block
  blk_hash_t pbft_chain_head_hash = pbft_chain1->getHeadHash();
  std::string pbft_chain_head_str = pbft_chain1->getJsonStr();
  db1->addPbftHeadToBatch(pbft_chain_head_hash, pbft_chain_head_str, batch);
  db1->commitWriteBatch(batch);
  EXPECT_EQ(pbft_chain1->getPbftChainSize(), 1);
  EXPECT_EQ(pbft_chain1->getPbftExecutedChainSize(), 0);
  // node1 cleanup block1 in PBFT unverified blocks table
  pbft_chain1->cleanupUnverifiedPbftBlocks(*pbft_block);
  bool find_erased_block = pbft_chain1->findUnverifiedPbftBlock(pbft_block->getBlockHash());
  ASSERT_FALSE(find_erased_block);

  nw1->onNewPbftBlock(*pbft_block);

  shared_ptr<PbftBlock> pbft_block_from_node2;
  shared_ptr<PbftBlock> pbft_block_from_node3;
  for (int i = 0; i < 300; i++) {
    // test timeout is 30 seconds
    pbft_block_from_node2 = pbft_chain2->getUnverifiedPbftBlock(pbft_block->getBlockHash());
    pbft_block_from_node3 = pbft_chain3->getUnverifiedPbftBlock(pbft_block->getBlockHash());
    if (pbft_block_from_node2 && pbft_block_from_node3) {
      // Both node2 and node3 get the pbft_block
      break;
    }
    taraxa::thisThreadSleepForMilliSeconds(100);
  }
  ASSERT_TRUE(pbft_block_from_node2);
  ASSERT_TRUE(pbft_block_from_node3);
  EXPECT_EQ(pbft_block_from_node2->getJsonStr(), pbft_block->getJsonStr());
  EXPECT_EQ(pbft_block_from_node3->getJsonStr(), pbft_block->getJsonStr());

  // node2 put block into pbft chain and store into DB
  auto db2 = node2->getDB();
  batch = db2->createWriteBatch();
  // Add PBFT block in DB
  db2->addPbftBlockToBatch(*pbft_block, batch);
  // Update PBFT chain
  pbft_chain2->updatePbftChain(pbft_block->getBlockHash());
  // Update PBFT chain head block
  pbft_chain_head_hash = pbft_chain2->getHeadHash();
  pbft_chain_head_str = pbft_chain2->getJsonStr();
  db2->addPbftHeadToBatch(pbft_chain_head_hash, pbft_chain_head_str, batch);
  db2->commitWriteBatch(batch);
  EXPECT_EQ(pbft_chain2->getPbftChainSize(), 1);
  EXPECT_EQ(pbft_chain2->getPbftExecutedChainSize(), 0);
  // node2 cleanup block1 in PBFT unverified blocks table
  pbft_chain2->cleanupUnverifiedPbftBlocks(*pbft_block);
  find_erased_block = pbft_chain2->findUnverifiedPbftBlock(pbft_block->getBlockHash());
  ASSERT_FALSE(find_erased_block);

  // node3 put block into pbft chain and store into DB
  auto db3 = node3->getDB();
  batch = db3->createWriteBatch();
  // Add PBFT block in DB
  db3->addPbftBlockToBatch(*pbft_block, batch);
  // Update PBFT chain
  pbft_chain3->updatePbftChain(pbft_block->getBlockHash());
  // Update PBFT chain head block
  pbft_chain_head_hash = pbft_chain3->getHeadHash();
  pbft_chain_head_str = pbft_chain3->getJsonStr();
  db3->addPbftHeadToBatch(pbft_chain_head_hash, pbft_chain_head_str, batch);
  db3->commitWriteBatch(batch);
  EXPECT_EQ(pbft_chain3->getPbftChainSize(), 1);
  EXPECT_EQ(pbft_chain3->getPbftExecutedChainSize(), 0);
  // node3 cleanup block1 in PBFT unverified blocks table
  pbft_chain3->cleanupUnverifiedPbftBlocks(*pbft_block);
  find_erased_block = pbft_chain3->findUnverifiedPbftBlock(pbft_block->getBlockHash());
  ASSERT_FALSE(find_erased_block);
}

}  // namespace taraxa::core_tests

using namespace taraxa;
int main(int argc, char **argv) {
  taraxa::static_init();
  auto logging = logger::createDefaultLoggingConfig();
  logging.verbosity = logger::Verbosity::Error;
  logging.channels["PBFT_CHAIN"] = logger::Verbosity::Error;

  addr_t node_addr;
  logger::InitLogging(logging, node_addr);

  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
