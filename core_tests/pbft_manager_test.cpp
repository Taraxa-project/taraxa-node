#include "pbft_manager.hpp"

#include <gtest/gtest.h>
#include <libdevcore/DBFactory.h>

#include "core_tests/util.hpp"
#include "create_samples.hpp"
#include "full_node.hpp"
#include "network.hpp"
#include "top.hpp"
#include "util.hpp"

namespace taraxa {
using namespace core_tests::util;
using core_tests::util::constants::TEST_TX_GAS_LIMIT;

const unsigned NUM_TRX = 200;
auto g_secret = dev::Secret(
    "3800b2875669d9b2053c1aff9224ecfdc411423aac5b5a73d7a45ced1c3b9dcd",
    dev::Secret::ConstructFromStringType::FromHex);
auto g_key_pair = dev::KeyPair(g_secret);
auto g_trx_signed_samples =
    samples::createSignedTrxSamples(0, NUM_TRX, g_secret);
auto g_mock_dag0 = samples::createMockDag0();

struct PbftManagerTest : core_tests::util::DBUsingTest<> {};

TEST_F(PbftManagerTest, pbft_manager_run_single_node) {
  const char *input[] = {"./build/main", "--conf_taraxa",
                         "./core_tests/conf/conf_taraxa1.json", "-v", "0"};
  Top top(5, input);
  auto node = top.getNode();

  // create a transaction
  auto nonce = val_t(0);
  auto coins_value = val_t(100);
  auto gas_price = val_t(2);
  auto receiver = addr_t("973ecb1c08c8eb5a7eaa0d3fd3aab7924f2838b0");
  auto data = bytes();
  Transaction trx_master_boot_node_to_receiver(nonce, coins_value, gas_price,
                                               TEST_TX_GAS_LIMIT, receiver,
                                               data, g_secret);
  node->insertTransaction(trx_master_boot_node_to_receiver);

  for (int i = 0; i < 100; i++) {
    // test timeout is 10 seconds
    if (node->getNumProposedBlocks() == 1) {
      break;
    }
    taraxa::thisThreadSleepForMilliSeconds(100);
  }
  EXPECT_EQ(node->getNumProposedBlocks(), 1);

  std::shared_ptr<PbftChain> pbft_chain = node->getPbftChain();
  // Vote DAG block
  for (int i = 0; i < 600; i++) {
    // test timeout is 60 seconds
    if (pbft_chain->getPbftChainSize() == 3) {
      break;
    }
    taraxa::thisThreadSleepForMilliSeconds(100);
  }
  EXPECT_EQ(pbft_chain->getPbftChainSize(), 3);

  std::cout << "Checking nodes sees 1 transaction..." << std::endl;

  bool checkpoint_passed = false;
  for (auto i = 0; i < 600; i++) {
    // test timeout is 60 seconds
    if (node->getNumTransactionExecuted() == 1) {
      checkpoint_passed = true;
      break;
    }
    taraxa::thisThreadSleepForMilliSeconds(100);
  }
  if (checkpoint_passed == false) {
    ASSERT_EQ(node->getNumTransactionExecuted(), 1);
  }

  EXPECT_EQ(node->getBalance(addr_t("de2b1203d72d3549ee2f733b00b2789414c7cea5"))
                .first,
            9007199254740991 - 100);

  EXPECT_EQ(node->getBalance(receiver).first, 100);
}

TEST_F(PbftManagerTest, pbft_manager_run_multi_nodes) {
  const char *input1[] = {"./build/main", "--conf_taraxa",
                          "./core_tests/conf/conf_taraxa1.json", "-v", "0"};
  const char *input2[] = {"./build/main", "--conf_taraxa",
                          "./core_tests/conf/conf_taraxa2.json", "-v", "0"};
  const char *input3[] = {"./build/main", "--conf_taraxa",
                          "./core_tests/conf/conf_taraxa3.json", "-v", "0"};
  Top top1(5, input1);
  std::cout << "Top1 created ..." << std::endl;

  Top top2(5, input2);
  std::cout << "Top2 created ..." << std::endl;

  Top top3(5, input3);
  std::cout << "Top3 created ..." << std::endl;

  auto node1 = top1.getNode();
  auto node2 = top2.getNode();
  auto node3 = top3.getNode();

  std::vector<std::shared_ptr<taraxa::FullNode>> nodes{node1, node2, node3};

  std::shared_ptr<Network> nw1 = node1->getNetwork();
  std::shared_ptr<Network> nw2 = node2->getNetwork();
  std::shared_ptr<Network> nw3 = node3->getNetwork();
  const int node_peers = 2;
  bool checkpoint_passed = false;
  const int timeout_val = 600;
  for (auto i = 0; i < timeout_val; i++) {
    // test timeout is 60 seconds
    if (nw1->getPeerCount() == node_peers &&
        nw2->getPeerCount() == node_peers &&
        nw3->getPeerCount() == node_peers) {
      checkpoint_passed = true;
      break;
    }
    taraxa::thisThreadSleepForMilliSeconds(100);
  }
  if (checkpoint_passed == false) {
    std::cout << "Timeout reached after " << timeout_val << " seconds..."
              << std::endl;
    ASSERT_EQ(node_peers, nw1->getPeerCount());
    ASSERT_EQ(node_peers, nw2->getPeerCount());
    ASSERT_EQ(node_peers, nw3->getPeerCount());
  }

  auto node1_addr = addr_t("de2b1203d72d3549ee2f733b00b2789414c7cea5");
  auto node2_addr = addr_t("973ecb1c08c8eb5a7eaa0d3fd3aab7924f2838b0");
  auto node3_addr = addr_t("4fae949ac2b72960fbe857b56532e2d3c8418d5e");

  // create a transaction transfer coins from node1 to node2
  auto coins_value2 = val_t(100);
  auto gas_price = val_t(2);
  auto data = bytes();
  Transaction trx_master_boot_node_to_node2(0, coins_value2, gas_price,
                                            TEST_TX_GAS_LIMIT, node2_addr, data,
                                            g_secret);
  node1->insertTransaction(trx_master_boot_node_to_node2);

  std::cout << "Checking all nodes see transaction from node 1 to node 2..."
            << std::endl;

  checkpoint_passed = false;
  for (auto i = 0; i < 600; i++) {
    // test timeout is 60 seconds
    if (node1->getNumTransactionExecuted() == 1 &&
        node2->getNumTransactionExecuted() == 1 &&
        node3->getNumTransactionExecuted() == 1) {
      checkpoint_passed = true;
      break;
    }
    taraxa::thisThreadSleepForMilliSeconds(100);
  }
  if (checkpoint_passed == false) {
    ASSERT_EQ(node1->getNumTransactionExecuted(), 1);
    ASSERT_EQ(node2->getNumTransactionExecuted(), 1);
    ASSERT_EQ(node3->getNumTransactionExecuted(), 1);
  }

  std::shared_ptr<PbftChain> pbft_chain1 = node1->getPbftChain();
  std::shared_ptr<PbftChain> pbft_chain2 = node2->getPbftChain();
  std::shared_ptr<PbftChain> pbft_chain3 = node3->getPbftChain();

  int pbft_chain_size = 3;

  EXPECT_EQ(pbft_chain1->getPbftChainSize(), pbft_chain_size);
  EXPECT_EQ(pbft_chain2->getPbftChainSize(), pbft_chain_size);
  EXPECT_EQ(pbft_chain3->getPbftChainSize(), pbft_chain_size);

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
                                            TEST_TX_GAS_LIMIT, node3_addr, data,
                                            g_secret);
  node1->insertTransaction(trx_master_boot_node_to_node3);

  std::cout << "Checking all nodes see transaction from node 1 to node 3..."
            << std::endl;

  checkpoint_passed = false;
  for (auto i = 0; i < 600; i++) {
    // test timeout is 60 seconds
    if (node1->getNumTransactionExecuted() == 2 &&
        node2->getNumTransactionExecuted() == 2 &&
        node3->getNumTransactionExecuted() == 2) {
      checkpoint_passed = true;
      break;
    }
    taraxa::thisThreadSleepForMilliSeconds(100);
  }
  if (checkpoint_passed == false) {
    ASSERT_EQ(node1->getNumTransactionExecuted(), 1);
    ASSERT_EQ(node2->getNumTransactionExecuted(), 1);
    ASSERT_EQ(node3->getNumTransactionExecuted(), 1);
  }

  pbft_chain_size = 5;
  // Vote DAG block
  for (auto i = 0; i < 600; i++) {
    // test timeout is 60 seconds
    if (pbft_chain1->getPbftChainSize() == pbft_chain_size &&
        pbft_chain2->getPbftChainSize() == pbft_chain_size &&
        pbft_chain3->getPbftChainSize() == pbft_chain_size) {
      break;
    }
    taraxa::thisThreadSleepForMilliSeconds(100);
  }
  EXPECT_EQ(pbft_chain1->getPbftChainSize(), pbft_chain_size);
  EXPECT_EQ(pbft_chain2->getPbftChainSize(), pbft_chain_size);
  EXPECT_EQ(pbft_chain3->getPbftChainSize(), pbft_chain_size);

  for (auto i = 0; i < nodes.size(); i++) {
    std::cout << "Checking account balances on node " << i << " ..."
              << std::endl;
    EXPECT_EQ(nodes[i]->getBalance(node1_addr).first, 9007199254740991 - 1100);
    EXPECT_EQ(nodes[i]->getBalance(node2_addr).first, 100);
    EXPECT_EQ(nodes[i]->getBalance(node3_addr).first, 1000);
  }
  std::unordered_set<blk_hash_t> unique_dag_block_hash_set;
  // PBFT second CS block
  blk_hash_t pbft_second_cs_block_hash = pbft_chain1->getLastPbftBlockHash();
  PbftBlock pbft_second_cs_block =
      pbft_chain1->getPbftBlockInChain(pbft_second_cs_block_hash);
  vec_blk_t dag_blocks_in_cs =
      pbft_second_cs_block.getScheduleBlock().getSchedule().dag_blks_order;
  // due to change of trx packing change, a trx can be packed in multiple blocks
  EXPECT_GE(dag_blocks_in_cs.size(), 1);
  for (auto &dag_block_hash : dag_blocks_in_cs) {
    ASSERT_FALSE(unique_dag_block_hash_set.count(dag_block_hash));
    unique_dag_block_hash_set.insert(dag_block_hash);
  }
  // PBFT second pivot block
  blk_hash_t pbft_second_pivot_block_hash =
      pbft_second_cs_block.getScheduleBlock().getPrevBlockHash();
  PbftBlock pbft_second_pivot_block =
      pbft_chain1->getPbftBlockInChain(pbft_second_pivot_block_hash);
  // PBFT first CS block
  blk_hash_t pbft_first_cs_block_hash =
      pbft_second_pivot_block.getPivotBlock().getPrevBlockHash();
  PbftBlock pbft_first_cs_block =
      pbft_chain1->getPbftBlockInChain(pbft_first_cs_block_hash);
  dag_blocks_in_cs =
      pbft_first_cs_block.getScheduleBlock().getSchedule().dag_blks_order;
  EXPECT_EQ(dag_blocks_in_cs.size(), 1);
  ASSERT_FALSE(unique_dag_block_hash_set.count(dag_blocks_in_cs[0]));
}

}  // namespace taraxa

int main(int argc, char **argv) {
  TaraxaStackTrace st;
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
  dev::db::setDatabaseKind(dev::db::DatabaseKind::RocksDB);
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
