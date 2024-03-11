#include <gtest/gtest.h>
#include <iostream>

#include "common/static_init.hpp"
#include "logger/logger.hpp"
#include "test_util/test_util.hpp"

namespace taraxa::core_tests {

struct PillarChainTest : NodesTest {};

TEST_F(PillarChainTest, pillar_chain) {
  auto node_cfgs = make_node_cfgs(3, 1, 20);
  for (auto& node_cfg : node_cfgs){
    node_cfg.genesis.state.hardforks.ficus_hf.block_num = 10000;
    node_cfg.genesis.state.hardforks.ficus_hf.pillar_block_periods = 2;
    node_cfg.genesis.state.hardforks.ficus_hf.pillar_chain_sync_periods = 1;
  }

  auto nodes = launch_nodes(node_cfgs);

  // Wait until nodes create 10 pbft blocks
  ASSERT_HAPPENS({20s, 500ms}, [&](auto &ctx) {
    for (const auto& node : nodes) {
      std::cout << "node->getPbftChain()->getPbftChainSize(): " << node->getPbftChain()->getPbftChainSize() << std::endl;
       WAIT_EXPECT_GE(ctx, node->getPbftChain()->getPbftChainSize(), 10)
    }
  });
}


TEST_F(PillarChainTest, pillar_chain_syncing) {
  std::cout << "TODO: implement";
  auto node_cfgs = make_node_cfgs(2, 2, 10);
  node_cfgs[1].is_light_node = true;
  node_cfgs[1].light_node_history = 4;
  node_cfgs[0].dag_expiry_limit = 15;
  node_cfgs[0].max_levels_per_period = 3;
  node_cfgs[1].dag_expiry_limit = 15;
  node_cfgs[1].max_levels_per_period = 3;
  auto nodes = launch_nodes(node_cfgs);
  uint64_t nonce = 0;
  size_t node1_chain_size = 0, node2_chain_size = 0;
  while (node2_chain_size < 20) {
    auto dummy_trx =
        std::make_shared<Transaction>(nonce++, 0, 2, 100000, bytes(), nodes[0]->getSecretKey(), nodes[0]->getAddress());
    // broadcast dummy transaction
    if (node1_chain_size == node2_chain_size) nodes[1]->getTransactionManager()->insertTransaction(dummy_trx);
    thisThreadSleepForMilliSeconds(500);
    node1_chain_size = nodes[0]->getPbftChain()->getPbftChainSizeExcludingEmptyPbftBlocks();
    node2_chain_size = nodes[1]->getPbftChain()->getPbftChainSizeExcludingEmptyPbftBlocks();
  }
  EXPECT_HAPPENS({10s, 1s}, [&](auto &ctx) {
    // Verify full node and light node sync without any issues
    WAIT_EXPECT_EQ(ctx, nodes[0]->getPbftChain()->getPbftChainSizeExcludingEmptyPbftBlocks(),
                   nodes[1]->getPbftChain()->getPbftChainSizeExcludingEmptyPbftBlocks())
  });
  uint32_t non_empty_counter = 0;
  uint64_t last_anchor_level;
  for (uint64_t i = 0; i < nodes[0]->getPbftChain()->getPbftChainSize(); i++) {
    const auto pbft_block = nodes[0]->getDB()->getPbftBlock(i);
    if (pbft_block && pbft_block->getPivotDagBlockHash() != kNullBlockHash) {
      non_empty_counter++;
      last_anchor_level = nodes[0]->getDB()->getDagBlock(pbft_block->getPivotDagBlockHash())->getLevel();
    }
  }
  uint32_t first_over_limit = 0;
  for (uint64_t i = 0; i < nodes[1]->getPbftChain()->getPbftChainSize(); i++) {
    const auto pbft_block = nodes[1]->getDB()->getPbftBlock(i);
    if (pbft_block && pbft_block->getPivotDagBlockHash() != kNullBlockHash) {
      auto dag_block = nodes[1]->getDB()->getDagBlock(pbft_block->getPivotDagBlockHash());
      if (dag_block && (dag_block->getLevel() + node_cfgs[0].dag_expiry_limit >= last_anchor_level)) {
        first_over_limit = i;
        break;
      }
    }
  }

  std::cout << "Non empty counter: " << non_empty_counter << std::endl;
  std::cout << "Last anchor level: " << last_anchor_level << std::endl;
  std::cout << "First over limit: " << first_over_limit << std::endl;

  // Verify light node does not delete non expired dag blocks
  EXPECT_TRUE(nodes[0]->getDB()->getPbftBlock(first_over_limit));
}

TEST_F(PillarChainTest, pillar_votes_syncing) {
  std::cout << "TODO: implement";
}

TEST_F(PillarChainTest, stakes_changes) {
  std::cout << "TODO: implement";
}

}  // namespace taraxa::core_tests

using namespace taraxa;
int main(int argc, char **argv) {
  taraxa::static_init();
  auto logging = logger::createDefaultLoggingConfig();
  logging.verbosity = logger::Verbosity::Info;

  addr_t node_addr;
  logger::InitLogging(logging, node_addr);

  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}