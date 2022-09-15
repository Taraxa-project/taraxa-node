#include "pbft/pbft_chain.hpp"

#include <gtest/gtest.h>

#include <atomic>
#include <iostream>
#include <vector>

#include "common/static_init.hpp"
#include "logger/logger.hpp"
#include "network/network.hpp"
#include "pbft/pbft_manager.hpp"
#include "util_test/samples.hpp"
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
  PbftBlock pbft_block1(prev_block_hash, dag_block_hash_as_pivot, blk_hash_t(), period, beneficiary, sk, {});

  auto rlp = pbft_block1.rlp(true);
  PbftBlock pbft_block2(rlp);
  EXPECT_EQ(pbft_block1.getJsonStr(), pbft_block2.getJsonStr());
}

TEST_F(PbftChainTest, pbft_db_test) {
  auto node_cfgs = make_node_cfgs(1);
  // There is no need to start a node for that db test
  auto node = create_nodes(node_cfgs).front();
  auto db = node->getDB();
  std::shared_ptr<PbftChain> pbft_chain = node->getPbftChain();
  blk_hash_t pbft_chain_head_hash = pbft_chain->getHeadHash();
  std::string pbft_head_from_db = db->getPbftHead(pbft_chain_head_hash);
  EXPECT_FALSE(pbft_head_from_db.empty());

  auto dag_genesis = node->getConfig().chain.dag_genesis_block.getHash();
  auto sk = node->getSecretKey();
  auto vrf_sk = node->getVrfSecretKey();
  SortitionConfig vdf_config(node_cfgs[0].chain.sortition);

  // generate PBFT block sample
  blk_hash_t prev_block_hash(0);
  level_t level = 1;
  vdf_sortition::VdfSortition vdf1(vdf_config, vrf_sk, getRlpBytes(level));
  vdf1.computeVdfSolution(vdf_config, dag_genesis.asBytes(), false);
  DagBlock blk1(dag_genesis, 1, {}, {}, {}, vdf1, sk);

  uint64_t period = 1;
  addr_t beneficiary(987);
  PbftBlock pbft_block(prev_block_hash, blk1.getHash(), blk_hash_t(), period, beneficiary, node->getSecretKey(), {});

  // put into pbft chain and store into DB
  auto batch = db->createWriteBatch();
  // Add PBFT block in DB
  std::vector<std::shared_ptr<Vote>> votes;

  PeriodData period_data(std::make_shared<PbftBlock>(pbft_block), votes);
  period_data.dag_blocks.push_back(blk1);
  db->savePeriodData(period_data, batch);

  // Update PBFT chain
  pbft_chain->updatePbftChain(pbft_block.getBlockHash(), pbft_block.getPivotDagBlockHash());
  // Update PBFT chain head block
  std::string pbft_chain_head_str = pbft_chain->getJsonStr();
  db->addPbftHeadToBatch(pbft_chain_head_hash, pbft_chain_head_str, batch);
  db->commitWriteBatch(batch);
  EXPECT_EQ(pbft_chain->getPbftChainSize(), 1);

  auto pbft_block_from_db = db->getPbftBlock(pbft_block.getBlockHash());
  EXPECT_EQ(pbft_block.getJsonStr(), pbft_block_from_db->getJsonStr());

  // check pbft genesis update in DB
  pbft_head_from_db = db->getPbftHead(pbft_chain_head_hash);
  EXPECT_EQ(pbft_head_from_db, pbft_chain->getJsonStr());
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
