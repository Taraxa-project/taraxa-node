/*
 * @Copyright: Taraxa.io
 * @Author: Qi Gao
 * @Date: 2019-05-08
 * @Last Modified by:
 * @Last Modified time:
 */

#include "pbft_manager.hpp"

#include <gtest/gtest.h>

#include "core_tests/util.hpp"
#include "create_samples.hpp"
#include "full_node.hpp"
#include "libdevcore/DBFactory.h"
#include "network.hpp"
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

// need change 2t+1 = 1 to test
TEST_F(PbftManagerTest, DISABLED_pbft_manager_run_single_node) {
  boost::asio::io_context context;
  FullNodeConfig cfg("./core_tests/conf/conf_taraxa1.json");
  val_t new_balance = 9007199254740991;  // Max Taraxa coins 2^53 - 1
  cfg.genesis_state.accounts[addr(cfg.node_secret)] = {new_balance};
  auto node(std::make_shared<taraxa::FullNode>(
      context, std::string("./core_tests/conf/conf_taraxa1.json")));

  std::shared_ptr<PbftManager> pbft_mgr = node->getPbftManager();

  node->setDebug(true);
  node->start(true);  // boot_node

  // stop pbft manager for test
  pbft_mgr->stop();

  // clean pbft queue and pbft chain
  std::shared_ptr<PbftChain> pbft_chain = node->getPbftChain();
  pbft_chain->cleanPbftQueue();
  pbft_chain->cleanPbftChain();

  std::unique_ptr<boost::asio::io_context::work> work(
      new boost::asio::io_context::work(context));

  boost::thread t([&context]() { context.run(); });

  std::shared_ptr<Network> nw = node->getNetwork();

  // node1 create transactions
  for (auto const& t : g_trx_signed_samples) {
    node->insertTransaction(t);
    taraxa::thisThreadSleepForMilliSeconds(50);
  }
  taraxa::thisThreadSleepForMilliSeconds(3000);
  EXPECT_GT(node->getNumProposedBlocks(), 0);

  // start pbft manager for test
  pbft_mgr->start();

  for (int i = 0; i < 300; i++) {
    // test timeout is 30 seconds
    if (pbft_chain->getPbftChainSize() == 2) {
      // stop pbft manager for test
      pbft_mgr->stop();
      break;
    }
    taraxa::thisThreadSleepForMilliSeconds(100);
  }
  EXPECT_EQ(pbft_chain->getPbftChainSize(), 2);

  work.reset();
  node->stop();
  t.join();
}

// need change 2t+1 = 3 to test
TEST_F(PbftManagerTest, pbft_manager_run_multi_nodes) {
  val_t new_balance = 9007199254740991;  // Max Taraxa coins 2^53 - 1
  vector<FullNodeConfig> cfgs;
  for (auto i = 1; i <= 3; ++i) {
    cfgs.emplace_back(fmt("./core_tests/conf/conf_taraxa%s.json", i));
  }
  for (auto& cfg : cfgs) {
    for (auto& cfg_other : cfgs) {
      cfg.genesis_state.accounts[addr(cfg_other.node_secret)] = {new_balance};
    }
  }
  auto node_count = 0;
  boost::asio::io_context context1;
  auto node1(std::make_shared<taraxa::FullNode>(context1, cfgs[node_count++]));
  boost::asio::io_context context2;
  auto node2(std::make_shared<taraxa::FullNode>(context2, cfgs[node_count++]));
  boost::asio::io_context context3;
  auto node3(std::make_shared<taraxa::FullNode>(context3, cfgs[node_count++]));

  std::shared_ptr<PbftManager> pbft_mgr1 = node1->getPbftManager();
  std::shared_ptr<PbftManager> pbft_mgr2 = node2->getPbftManager();
  std::shared_ptr<PbftManager> pbft_mgr3 = node3->getPbftManager();

  node1->setDebug(true);
  node2->setDebug(true);
  node3->setDebug(true);
  node1->start(true);  // boot_node
  node2->start(false);
  node3->start(false);

  // stop pbft manager for test
  pbft_mgr1->stop();
  pbft_mgr2->stop();
  pbft_mgr3->stop();

  // clean pbft queue and pbft chain
  std::shared_ptr<PbftChain> pbft_chain1 = node1->getPbftChain();
  std::shared_ptr<PbftChain> pbft_chain2 = node2->getPbftChain();
  std::shared_ptr<PbftChain> pbft_chain3 = node3->getPbftChain();
  pbft_chain1->cleanPbftQueue();
  pbft_chain2->cleanPbftQueue();
  pbft_chain3->cleanPbftQueue();
  pbft_chain1->cleanPbftChain();
  pbft_chain2->cleanPbftChain();
  pbft_chain3->cleanPbftChain();

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

  // nodes create transactions
  for (auto const& t : g_trx_signed_samples) {
    node1->insertTransaction(t);
    node2->insertTransaction(t);
    node3->insertTransaction(t);
    taraxa::thisThreadSleepForMilliSeconds(50);
  }
  taraxa::thisThreadSleepForMilliSeconds(3000);
  EXPECT_GT(node1->getNumProposedBlocks(), 0);
  EXPECT_GT(node2->getNumProposedBlocks(), 0);
  EXPECT_GT(node3->getNumProposedBlocks(), 0);

  // start pbft manager for test
  pbft_mgr1->start();
  pbft_mgr2->start();
  pbft_mgr3->start();

  for (int i = 0; i < 300; i++) {
    // test timeout is 30 seconds
    if (pbft_chain1->getPbftChainSize() == 2 &&
        pbft_chain2->getPbftChainSize() == 2 &&
        pbft_chain3->getPbftChainSize() == 2) {
      // stop pbft manager for test
      pbft_mgr1->stop();
      pbft_mgr2->stop();
      pbft_mgr3->stop();
      break;
    }
    taraxa::thisThreadSleepForMilliSeconds(100);
  }
  EXPECT_EQ(pbft_chain1->getPbftChainSize(), 2);
  EXPECT_EQ(pbft_chain2->getPbftChainSize(), 2);
  EXPECT_EQ(pbft_chain3->getPbftChainSize(), 2);

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

}  // namespace taraxa

int main(int argc, char** argv) {
  TaraxaStackTrace st;
  dev::LoggingOptions logOptions;
  logOptions.verbosity = dev::VerbosityError;
  logOptions.includeChannels.push_back("PBFT_MGR");
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