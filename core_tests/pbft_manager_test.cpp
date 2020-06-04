#include "pbft_manager.hpp"

#include <gtest/gtest.h>

#include "core_tests/util.hpp"
#include "create_samples.hpp"
#include "full_node.hpp"
#include "network.hpp"
#include "static_init.hpp"
#include "top.hpp"
#include "util.hpp"
#include "util/lazy.hpp"

namespace taraxa {
using namespace core_tests::util;
using core_tests::util::constants::TEST_TX_GAS_LIMIT;
using ::taraxa::util::lazy::Lazy;

const unsigned NUM_TRX = 200;
auto g_secret = Lazy([] {
  return dev::Secret(
      "3800b2875669d9b2053c1aff9224ecfdc411423aac5b5a73d7a45ced1c3b9dcd",
      dev::Secret::ConstructFromStringType::FromHex);
});
auto g_key_pair = Lazy([] { return dev::KeyPair(g_secret); });
auto g_trx_signed_samples =
    Lazy([] { return samples::createSignedTrxSamples(0, NUM_TRX, g_secret); });
auto g_mock_dag0 = Lazy([] { return samples::createMockDag0(); });

struct PbftManagerTest : core_tests::util::DBUsingTest<> {};

TEST_F(PbftManagerTest, pbft_manager_run_single_node) {
  auto tops = createNodesAndVerifyConnection(1);
  auto node = tops.second[0];

  // create a transaction
  auto coins_value = val_t(100);
  auto gas_price = val_t(2);
  auto receiver = addr_t("973ecb1c08c8eb5a7eaa0d3fd3aab7924f2838b0");
  auto data = bytes();
  Transaction trx_master_boot_node_to_receiver(0, coins_value, gas_price,
                                               TEST_TX_GAS_LIMIT, data,
                                               g_secret, receiver);
  node->insertTransaction(trx_master_boot_node_to_receiver, false);

  for (int i = 0; i < 100; i++) {
    // test timeout is 10 seconds
    if (node->getNumProposedBlocks() == 1) {
      break;
    }
    taraxa::thisThreadSleepForMilliSeconds(500);
  }
  EXPECT_EQ(node->getNumProposedBlocks(), 1);

  std::shared_ptr<PbftChain> pbft_chain = node->getPbftChain();
  // Vote DAG block
  int pbft_chain_size = 1;
  for (int i = 0; i < 600; i++) {
    // test timeout is 60 seconds
    if (pbft_chain->getPbftChainSize() == pbft_chain_size) {
      break;
    }
    taraxa::thisThreadSleepForMilliSeconds(500);
  }
  EXPECT_EQ(pbft_chain->getPbftChainSize(), pbft_chain_size);

  std::cout << "Checking nodes sees 1 transaction..." << std::endl;
  ASSERT_EQ(node->getNumTransactionExecuted(), 1);
  EXPECT_EQ(node->getBalance(addr_t("de2b1203d72d3549ee2f733b00b2789414c7cea5"))
                .first,
            9007199254740991 - 100);
  EXPECT_EQ(node->getBalance(receiver).first, 100);
}

TEST_F(PbftManagerTest, pbft_manager_run_multi_nodes) {
  auto tops = createNodesAndVerifyConnection(3);
  auto &nodes = tops.second;

  auto node1_addr = addr_t("de2b1203d72d3549ee2f733b00b2789414c7cea5");
  auto node2_addr = addr_t("973ecb1c08c8eb5a7eaa0d3fd3aab7924f2838b0");
  auto node3_addr = addr_t("4fae949ac2b72960fbe857b56532e2d3c8418d5e");

  // create a transaction transfer coins from node1 to node2
  auto coins_value2 = val_t(100);
  auto gas_price = val_t(2);
  auto data = bytes();
  Transaction trx_master_boot_node_to_node2(0, coins_value2, gas_price,
                                            TEST_TX_GAS_LIMIT, data, g_secret,
                                            node2_addr);
  // broadcast trx and insert
  nodes[0]->insertTransaction(trx_master_boot_node_to_node2, false);

  std::cout << "Checking all nodes see transaction from node 1 to node 2..."
            << std::endl;

  bool checkpoint_passed = false;
  for (auto i = 0; i < 600; i++) {
    checkpoint_passed = true;
    // test timeout is 60 seconds
    for (auto i = 0; i < nodes.size(); i++) {
      if (nodes[i]->getNumTransactionExecuted() == 0) {
        checkpoint_passed = false;
      }
    }
    if (checkpoint_passed) break;
    taraxa::thisThreadSleepForMilliSeconds(500);
  }

  if (checkpoint_passed == false) {
    for (auto i = 0; i < nodes.size(); i++) {
      ASSERT_EQ(nodes[i]->getNumTransactionExecuted(), 1);
    }
  }

  int pbft_chain_size = 1;
  for (auto i = 0; i < nodes.size(); i++) {
    EXPECT_EQ(nodes[i]->getPbftChainSize(), pbft_chain_size);
  }

  for (auto i = 0; i < nodes.size(); i++) {
    std::cout << "Checking account balances on node " << i << " ..."
              << std::endl;
    EXPECT_EQ(nodes[i]->getBalance(node1_addr).first, 9007199254740991 - 100);
    EXPECT_EQ(nodes[i]->getBalance(node2_addr).first, 100);
    EXPECT_EQ(nodes[i]->getBalance(node3_addr).first, 0);
  }

  // create a transaction transfer coins from node1 to node3
  auto coins_value3 = val_t(1000);
  Transaction trx_master_boot_node_to_node3(1, coins_value3, gas_price,
                                            TEST_TX_GAS_LIMIT, data, g_secret,
                                            node3_addr);
  // broadcast trx and insert
  nodes[0]->insertTransaction(trx_master_boot_node_to_node3, false);

  std::cout << "Checking all nodes see transaction from node 1 to node 3..."
            << std::endl;

  checkpoint_passed = false;
  for (auto i = 0; i < 600; i++) {
    checkpoint_passed = true;
    // test timeout is 60 seconds
    for (auto i = 0; i < nodes.size(); i++) {
      if (nodes[i]->getNumTransactionExecuted() != 2) {
        checkpoint_passed = false;
      }
    }
    if (checkpoint_passed) break;
    taraxa::thisThreadSleepForMilliSeconds(500);
  }

  if (checkpoint_passed == false) {
    for (auto i = 0; i < nodes.size(); i++) {
      ASSERT_EQ(nodes[i]->getNumTransactionExecuted(), 1);
    }
  }

  pbft_chain_size = 2;
  // Vote DAG block
  for (auto i = 0; i < 600; i++) {
    // test timeout is 60 seconds
    if (nodes[0]->getPbftChainSize() == pbft_chain_size &&
        nodes[1]->getPbftChainSize() == pbft_chain_size &&
        nodes[2]->getPbftChainSize() == pbft_chain_size) {
      break;
    }
    taraxa::thisThreadSleepForMilliSeconds(500);
  }
  for (auto i = 0; i < nodes.size(); i++) {
    EXPECT_EQ(nodes[i]->getPbftChainSize(), pbft_chain_size);
  }

  for (auto i = 0; i < nodes.size(); i++) {
    std::cout << "Checking account balances on node " << i << " ..."
              << std::endl;
    EXPECT_EQ(nodes[i]->getBalance(node1_addr).first, 9007199254740991 - 1100);
    EXPECT_EQ(nodes[i]->getBalance(node2_addr).first, 100);
    EXPECT_EQ(nodes[i]->getBalance(node3_addr).first, 1000);
  }
  std::unordered_set<blk_hash_t> unique_dag_block_hash_set;
  // PBFT second block
  blk_hash_t pbft_second_block_hash =
      nodes[0]->getPbftChain()->getLastPbftBlockHash();
  PbftBlock pbft_second_block =
      nodes[0]->getPbftChain()->getPbftBlockInChain(pbft_second_block_hash);
  vec_blk_t dag_blocks_hash_in_schedule =
      pbft_second_block.getSchedule().dag_blks_order;
  // due to change of trx packing change, a trx can be packed in multiple blocks
  EXPECT_GE(dag_blocks_hash_in_schedule.size(), 1);
  for (auto &dag_block_hash : dag_blocks_hash_in_schedule) {
    ASSERT_FALSE(unique_dag_block_hash_set.count(dag_block_hash));
    unique_dag_block_hash_set.insert(dag_block_hash);
  }
  // PBFT first block hash
  blk_hash_t pbft_first_block_hash = pbft_second_block.getPrevBlockHash();
  // PBFT first block
  PbftBlock pbft_first_block =
      nodes[0]->getPbftChain()->getPbftBlockInChain(pbft_first_block_hash);
  dag_blocks_hash_in_schedule = pbft_first_block.getSchedule().dag_blks_order;
  // due to change of trx packing change, a trx can be packed in multiple blocks
  EXPECT_GE(dag_blocks_hash_in_schedule.size(), 1);
  for (auto &dag_block_hash : dag_blocks_hash_in_schedule) {
    ASSERT_FALSE(unique_dag_block_hash_set.count(dag_block_hash));
    unique_dag_block_hash_set.insert(dag_block_hash);
  }
}

}  // namespace taraxa

int main(int argc, char **argv) {
  taraxa::static_init();
  dev::LoggingOptions logOptions;
  logOptions.verbosity = dev::VerbosityError;
  logOptions.includeChannels.push_back("PBFT_CHAIN");
  logOptions.includeChannels.push_back("PBFT_MGR");
  logOptions.includeChannels.push_back("VOTE_MGR");
  logOptions.includeChannels.push_back("SORTI");
  logOptions.includeChannels.push_back("EXETOR");
  logOptions.includeChannels.push_back("BLK_PP");
  logOptions.includeChannels.push_back("FULLND");
  logOptions.includeChannels.push_back("TRXMGR");
  logOptions.includeChannels.push_back("TRXQU");
  logOptions.includeChannels.push_back("TARCAP");
  dev::setupLogging(logOptions);
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
