/*
 * @Copyright: Taraxa.io
 * @Author: Qi Gao
 * @Date: 2019-05-08
 * @Last Modified by: Qi Gao
 * @Last Modified time: 2019-08-16
 */

#include "pbft_manager.hpp"

#include <gtest/gtest.h>

#include "core_tests/util.hpp"
#include "create_samples.hpp"
#include "full_node.hpp"
#include "libdevcore/DBFactory.h"
#include "network.hpp"
#include "top.hpp"
#include "util.hpp"

namespace taraxa {
using namespace core_tests::util;

const unsigned NUM_TRX = 200;
auto g_secret = dev::Secret(
    "3800b2875669d9b2053c1aff9224ecfdc411423aac5b5a73d7a45ced1c3b9dcd",
    dev::Secret::ConstructFromStringType::FromHex);
auto g_key_pair = dev::KeyPair(g_secret);
auto g_trx_signed_samples =
    samples::createSignedTrxSamples(0, NUM_TRX, g_secret);
auto g_mock_dag0 = samples::createMockDag0();

struct PbftManagerTest : public DBUsingTest<> {};

TEST_F(PbftManagerTest, pbft_manager_run_single_node) {
  const char *input[] = {"./build/main", "--conf_taraxa",
                         "./core_tests/conf/conf_taraxa1.json", "-v", "0"};
  Top top(5, input);
  EXPECT_TRUE(top.isActive());
  auto node = top.getNode();
  EXPECT_NE(node, nullptr);

  std::shared_ptr<PbftChain> pbft_chain = node->getPbftChain();
  // Vote DAG genesis
  for (int i = 0; i < 600; i++) {
    // test timeout is 60 seconds
    if (pbft_chain->getPbftChainSize() == 3) {
      break;
    }
    taraxa::thisThreadSleepForMilliSeconds(100);
  }
  EXPECT_EQ(pbft_chain->getPbftChainSize(), 3);

  top.kill();
}

TEST_F(PbftManagerTest, pbft_manager_run_multi_nodes) {
  // copy main2, main3
  try {
    std::cout << "Copying main2 ..." << std::endl;
    system("cp ./build/main ./build/main2");
    std::cout << "Copying main3 ..." << std::endl;
    system("cp ./build/main ./build/main3");
  } catch (std::exception &e) {
    std::cerr << e.what() << std::endl;
  }

  const char *input1[] = {"./build/main", "--conf_taraxa",
                          "./core_tests/conf/conf_taraxa1.json", "-v", "0"};
  const char *input2[] = {"./build/main2", "--conf_taraxa",
                          "./core_tests/conf/conf_taraxa2.json", "-v", "0"};
  const char *input3[] = {"./build/main3", "--conf_taraxa",
                          "./core_tests/conf/conf_taraxa3.json", "-v", "0"};
  Top top1(5, input1);
  EXPECT_TRUE(top1.isActive());
  std::cout << "Top1 created ..." << std::endl;

  Top top2(5, input2);
  EXPECT_TRUE(top2.isActive());
  std::cout << "Top2 created ..." << std::endl;

  Top top3(5, input3);
  EXPECT_TRUE(top3.isActive());
  std::cout << "Top3 created ..." << std::endl;

  auto node1 = top1.getNode();
  auto node2 = top2.getNode();
  auto node3 = top3.getNode();
  EXPECT_NE(node1, nullptr);
  EXPECT_NE(node2, nullptr);
  EXPECT_NE(node3, nullptr);

  std::shared_ptr<Network> nw1 = node1->getNetwork();
  std::shared_ptr<Network> nw2 = node2->getNetwork();
  std::shared_ptr<Network> nw3 = node3->getNetwork();
  const int node_peers = 2;
  for (int i = 0; i < 600; i++) {
    // test timeout is 60 seconds
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

  std::shared_ptr<PbftChain> pbft_chain1 = node1->getPbftChain();
  std::shared_ptr<PbftChain> pbft_chain2 = node2->getPbftChain();
  std::shared_ptr<PbftChain> pbft_chain3 = node3->getPbftChain();
  const int pbft_chain_size = 3;
  // Vote DAG genesis
  for (int i = 0; i < 600; i++) {
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

  top3.kill();
  top2.kill();
  top1.kill();
  // delete main2, main3
  try {
    std::cout << "main3 deleted ..." << std::endl;
    system("rm -f ./build/main3");
    std::cout << "main2 deleted ..." << std::endl;
    system("rm -f ./build/main2");
  } catch (std::exception &e) {
    std::cerr << e.what() << std::endl;
  }
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
  // use the in-memory db so test will not affect other each other through
  // persistent storage
  dev::db::setDatabaseKind(dev::db::DatabaseKind::MemoryDB);
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
