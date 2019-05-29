/*
 * @Copyright: Taraxa.io
 * @Author: Qi Gao
 * @Date: 2019-05-08
 * @Last Modified by:
 * @Last Modified time:
 */

#include "pbft_manager.hpp"

#include <gtest/gtest.h>

#include "create_samples.hpp"
#include "full_node.hpp"
#include "libdevcore/DBFactory.h"
#include "network.hpp"

namespace taraxa {

const unsigned NUM_TRX = 200;
auto g_secret = dev::Secret(
    "3800b2875669d9b2053c1aff9224ecfdc411423aac5b5a73d7a45ced1c3b9dcd",
    dev::Secret::ConstructFromStringType::FromHex);
auto g_key_pair = dev::KeyPair(g_secret);
auto g_trx_signed_samples =
    samples::createSignedTrxSamples(0, NUM_TRX, g_secret);
auto g_mock_dag0 = samples::createMockDag0();

// need change 2t+1 = 1 to test
TEST(PbftManager, DISABLED_pbft_manager_run_single_node) {
  boost::asio::io_context context;
  auto node(std::make_shared<taraxa::FullNode>(
      context, std::string("./core_tests/conf_taraxa1.json")));

  bal_t new_balance = 9007199254740991;  // Max Taraxa coins 2^53 - 1
  addr_t account_address = node->getAddress();
  node->setBalance(account_address, new_balance);

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
TEST(PbftManager, DISABLED_pbft_manager_run_multi_nodes) {
  boost::asio::io_context context1;
  auto node1(std::make_shared<taraxa::FullNode>(
      context1, std::string("./core_tests/conf_taraxa1.json")));
  boost::asio::io_context context2;
  auto node2(std::make_shared<taraxa::FullNode>(
      context2, std::string("./core_tests/conf_taraxa2.json")));
  boost::asio::io_context context3;
  auto node3(std::make_shared<taraxa::FullNode>(
      context3, std::string("./core_tests/conf_taraxa3.json")));

  bal_t new_balance = 9007199254740991;  // Max Taraxa coins 2^53 - 1
  addr_t account_address1 = node1->getAddress();
  node1->setBalance(account_address1, new_balance);
  node2->setBalance(account_address1, new_balance);
  node3->setBalance(account_address1, new_balance);
  addr_t account_address2 = node2->getAddress();
  node1->setBalance(account_address2, new_balance);
  node2->setBalance(account_address2, new_balance);
  node3->setBalance(account_address2, new_balance);
  addr_t account_address3 = node3->getAddress();
  node1->setBalance(account_address3, new_balance);
  node2->setBalance(account_address3, new_balance);
  node3->setBalance(account_address3, new_balance);

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

TEST(PbftManager, DISABLED_pbft_manager_workflow_simulate_steps) {
  // TODO: need fix
  boost::asio::io_context context1;
  auto node1(std::make_shared<taraxa::FullNode>(
      context1, std::string("./core_tests/conf_taraxa1.json")));
  boost::asio::io_context context2;
  auto node2(std::make_shared<taraxa::FullNode>(
      context2, std::string("./core_tests/conf_taraxa2.json")));
  boost::asio::io_context context3;
  auto node3(std::make_shared<taraxa::FullNode>(
      context3, std::string("./core_tests/conf_taraxa3.json")));
  node1->setDebug(true);
  node2->setDebug(true);
  node3->setDebug(true);
  node1->start(true);  // boot_node
  node2->start(false);
  node3->start(false);

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

  bal_t new_balance = 9007199254740991;  // Max Taraxa coins 2^53 - 1
  addr_t account_address1 = node1->getAddress();
  node1->setBalance(account_address1, new_balance);
  node2->setBalance(account_address1, new_balance);
  node3->setBalance(account_address1, new_balance);
  addr_t account_address2 = node2->getAddress();
  node1->setBalance(account_address2, new_balance);
  node2->setBalance(account_address2, new_balance);
  node3->setBalance(account_address2, new_balance);
  addr_t account_address3 = node3->getAddress();
  node1->setBalance(account_address3, new_balance);
  node2->setBalance(account_address3, new_balance);
  node3->setBalance(account_address3, new_balance);

  std::shared_ptr<PbftManager> pbft_mgr1 = node1->getPbftManager();
  std::shared_ptr<PbftManager> pbft_mgr2 = node2->getPbftManager();
  std::shared_ptr<PbftManager> pbft_mgr3 = node3->getPbftManager();
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

  // period 1, step 1
  // node1 propose pbft anchor block, 1 propose vote
  pbft_mgr1->setPbftStep(1);
  pbft_mgr1->setPbftPeriod(1);
  pbft_mgr1->start();

  int current_pbft_queue_size = 1;
  for (int i = 0; i < 3000; i++) {
    // test timeout is 3 seconds
    if (node1->getPbftQueueSize() == current_pbft_queue_size &&
        node1->getVoteQueueSize() == current_pbft_queue_size) {
      break;
    }
    taraxa::thisThreadSleepForMilliSeconds(1);
  }
  pbft_mgr1->stop();
  EXPECT_EQ(node1->getPbftQueueSize(), current_pbft_queue_size);
  EXPECT_EQ(node1->getVoteQueueSize(), current_pbft_queue_size);

  for (int i = 0; i < 300; i++) {
    // test timeout is 30 seconds
    if (node2->getPbftQueueSize() == current_pbft_queue_size &&
        node3->getPbftQueueSize() == current_pbft_queue_size) {
      break;
    }
    taraxa::thisThreadSleepForMilliSeconds(100);
  }
  EXPECT_EQ(node2->getPbftQueueSize(), current_pbft_queue_size);
  EXPECT_EQ(node3->getPbftQueueSize(), current_pbft_queue_size);

  for (int i = 0; i < 300; i++) {
    // test timeout is 30 seconds
    if (node2->getVoteQueueSize() == current_pbft_queue_size &&
        node3->getVoteQueueSize() == current_pbft_queue_size) {
      break;
    }
    taraxa::thisThreadSleepForMilliSeconds(100);
  }
  EXPECT_EQ(node2->getVoteQueueSize(), current_pbft_queue_size);
  EXPECT_EQ(node3->getVoteQueueSize(), current_pbft_queue_size);

  // period 1, step 2
  // node1 soft vote the pbft anchor block, 1 propose vote 1 soft vote
  pbft_mgr1->setPbftStep(2);
  pbft_mgr1->setPbftPeriod(1);
  pbft_mgr1->start();

  current_pbft_queue_size = 2;
  for (int i = 0; i < 3000; i++) {
    // test timeout is 3 seconds
    if (node1->getVoteQueueSize() == current_pbft_queue_size) {
      break;
    }
    taraxa::thisThreadSleepForMilliSeconds(1);
  }
  pbft_mgr1->stop();
  EXPECT_EQ(node1->getVoteQueueSize(), current_pbft_queue_size);

  for (int i = 0; i < 300; i++) {
    // test timeout is 30 seconds
    if (node2->getVoteQueueSize() == current_pbft_queue_size &&
        node3->getVoteQueueSize() == current_pbft_queue_size) {
      break;
    }
    taraxa::thisThreadSleepForMilliSeconds(100);
  }
  EXPECT_EQ(node2->getVoteQueueSize(), current_pbft_queue_size);
  EXPECT_EQ(node3->getVoteQueueSize(), current_pbft_queue_size);
  // node2 soft vote the pbft anchor block, 1 propose vote 2 soft votes
  pbft_mgr2->setPbftStep(2);
  pbft_mgr2->setPbftPeriod(1);
  pbft_mgr2->start();

  current_pbft_queue_size = 3;
  for (int i = 0; i < 3000; i++) {
    // test timeout is 3 seconds
    if (node2->getVoteQueueSize() == current_pbft_queue_size) {
      break;
    }
    taraxa::thisThreadSleepForMilliSeconds(1);
  }
  pbft_mgr2->stop();
  EXPECT_EQ(node2->getVoteQueueSize(), current_pbft_queue_size);

  for (int i = 0; i < 300; i++) {
    // test timeout is 30 seconds
    if (node1->getVoteQueueSize() == current_pbft_queue_size &&
        node3->getVoteQueueSize() == current_pbft_queue_size) {
      break;
    }
    taraxa::thisThreadSleepForMilliSeconds(100);
  }
  EXPECT_EQ(node1->getVoteQueueSize(), current_pbft_queue_size);
  EXPECT_EQ(node3->getVoteQueueSize(), current_pbft_queue_size);
  // node3 soft vote the pbft anchor block, 1 propose vote 3 soft votes
  pbft_mgr3->setPbftStep(2);
  pbft_mgr3->setPbftPeriod(1);
  pbft_mgr3->start();

  current_pbft_queue_size = 4;
  for (int i = 0; i < 3000; i++) {
    // test timeout is 3 seconds
    if (node3->getVoteQueueSize() == current_pbft_queue_size) {
      break;
    }
    taraxa::thisThreadSleepForMilliSeconds(1);
  }
  pbft_mgr3->stop();
  EXPECT_EQ(node3->getVoteQueueSize(), current_pbft_queue_size);

  for (int i = 0; i < 300; i++) {
    // test timeout is 30 seconds
    if (node1->getVoteQueueSize() == current_pbft_queue_size &&
        node2->getVoteQueueSize() == current_pbft_queue_size) {
      break;
    }
    taraxa::thisThreadSleepForMilliSeconds(100);
  }
  EXPECT_EQ(node1->getVoteQueueSize(), current_pbft_queue_size);
  EXPECT_EQ(node2->getVoteQueueSize(), current_pbft_queue_size);

  // period 1, step 3
  // node1 cert vote for the pbft anchor block, 1 propose vote 3 soft votes 1
  // cert vote
  pbft_mgr1->setPbftStep(3);
  pbft_mgr1->setPbftPeriod(1);
  pbft_mgr1->start();

  current_pbft_queue_size = 5;
  for (int i = 0; i < 3000; i++) {
    // test timeout is 3 seconds
    if (node1->getVoteQueueSize() == current_pbft_queue_size) {
      break;
    }
    taraxa::thisThreadSleepForMilliSeconds(1);
  }
  pbft_mgr1->stop();
  EXPECT_EQ(node1->getVoteQueueSize(), current_pbft_queue_size);

  for (int i = 0; i < 300; i++) {
    // test timeout is 30 seconds
    if (node2->getVoteQueueSize() == current_pbft_queue_size &&
        node3->getVoteQueueSize() == current_pbft_queue_size) {
      break;
    }
    taraxa::thisThreadSleepForMilliSeconds(100);
  }
  EXPECT_EQ(node2->getVoteQueueSize(), current_pbft_queue_size);
  EXPECT_EQ(node3->getVoteQueueSize(), current_pbft_queue_size);
  // node2 cert vote for the pbft anchor block, 1 propose vote 3 soft votes 2
  // cert votes
  pbft_mgr2->setPbftStep(3);
  pbft_mgr2->setPbftPeriod(1);
  pbft_mgr2->start();

  current_pbft_queue_size = 6;
  for (int i = 0; i < 3000; i++) {
    // test timeout is 3 seconds
    if (node2->getVoteQueueSize() == current_pbft_queue_size) {
      break;
    }
    taraxa::thisThreadSleepForMilliSeconds(1);
  }
  pbft_mgr2->stop();
  EXPECT_EQ(node2->getVoteQueueSize(), current_pbft_queue_size);

  for (int i = 0; i < 300; i++) {
    // test timeout is 30 seconds
    if (node1->getVoteQueueSize() == current_pbft_queue_size &&
        node3->getVoteQueueSize() == current_pbft_queue_size) {
      break;
    }
    taraxa::thisThreadSleepForMilliSeconds(100);
  }
  EXPECT_EQ(node1->getVoteQueueSize(), current_pbft_queue_size);
  EXPECT_EQ(node3->getVoteQueueSize(), current_pbft_queue_size);
  // node3 cert vote for the pbft anchor block, 1 propose vote 3 soft votes 3
  // cert votes
  pbft_mgr3->setPbftStep(3);
  pbft_mgr3->setPbftPeriod(1);
  pbft_mgr3->start();

  current_pbft_queue_size = 7;
  for (int i = 0; i < 3000; i++) {
    // test timeout is 3 seconds
    if (node3->getVoteQueueSize() == current_pbft_queue_size) {
      break;
    }
    taraxa::thisThreadSleepForMilliSeconds(1);
  }
  pbft_mgr3->stop();
  EXPECT_EQ(node3->getVoteQueueSize(), current_pbft_queue_size);

  for (int i = 0; i < 300; i++) {
    // test timeout is 30 seconds
    if (node1->getVoteQueueSize() == current_pbft_queue_size &&
        node2->getVoteQueueSize() == current_pbft_queue_size) {
      break;
    }
    taraxa::thisThreadSleepForMilliSeconds(100);
  }
  EXPECT_EQ(node1->getVoteQueueSize(), current_pbft_queue_size);
  EXPECT_EQ(node2->getVoteQueueSize(), current_pbft_queue_size);

  // period 1, step 4
  // node1 next vote for the pbft anchor block, 1 propose vote 3 soft votes 3
  // cert votes, 1 next vote
  pbft_mgr1->setPbftStep(4);
  pbft_mgr1->setPbftPeriod(1);
  pbft_mgr1->start();

  current_pbft_queue_size = 8;
  for (int i = 0; i < 3000; i++) {
    // test timeout is 3 seconds
    if (node1->getVoteQueueSize() == current_pbft_queue_size) {
      break;
    }
    taraxa::thisThreadSleepForMilliSeconds(1);
  }
  pbft_mgr1->stop();
  EXPECT_EQ(node1->getVoteQueueSize(), current_pbft_queue_size);

  for (int i = 0; i < 300; i++) {
    // test timeout is 30 seconds
    if (node2->getVoteQueueSize() == current_pbft_queue_size &&
        node3->getVoteQueueSize() == current_pbft_queue_size) {
      break;
    }
    taraxa::thisThreadSleepForMilliSeconds(100);
  }
  EXPECT_EQ(node2->getVoteQueueSize(), current_pbft_queue_size);
  EXPECT_EQ(node3->getVoteQueueSize(), current_pbft_queue_size);
  // node2 cert vote for the pbft anchor block, 1 propose vote 3 soft votes 3
  // cert votes, 2 next votes
  pbft_mgr2->setPbftStep(4);
  pbft_mgr2->setPbftPeriod(1);
  pbft_mgr2->start();

  current_pbft_queue_size = 9;
  for (int i = 0; i < 3000; i++) {
    // test timeout is 3 seconds
    if (node2->getVoteQueueSize() == current_pbft_queue_size) {
      break;
    }
    taraxa::thisThreadSleepForMilliSeconds(1);
  }
  pbft_mgr2->stop();
  EXPECT_EQ(node2->getVoteQueueSize(), current_pbft_queue_size);

  for (int i = 0; i < 300; i++) {
    // test timeout is 30 seconds
    if (node1->getVoteQueueSize() == current_pbft_queue_size &&
        node3->getVoteQueueSize() == current_pbft_queue_size) {
      break;
    }
    taraxa::thisThreadSleepForMilliSeconds(100);
  }
  EXPECT_EQ(node1->getVoteQueueSize(), current_pbft_queue_size);
  EXPECT_EQ(node3->getVoteQueueSize(), current_pbft_queue_size);
  // node3 cert vote for the pbft anchor block, 1 propose vote 3 soft votes 3
  // cert votes, 3 next votes
  pbft_mgr3->setPbftStep(4);
  pbft_mgr3->setPbftPeriod(1);
  pbft_mgr3->start();

  current_pbft_queue_size = 10;
  for (int i = 0; i < 3000; i++) {
    // test timeout is 3 seconds
    if (node3->getVoteQueueSize() == current_pbft_queue_size) {
      break;
    }
    taraxa::thisThreadSleepForMilliSeconds(1);
  }
  pbft_mgr3->stop();
  EXPECT_EQ(node3->getVoteQueueSize(), current_pbft_queue_size);

  for (int i = 0; i < 300; i++) {
    // test timeout is 30 seconds
    if (node1->getVoteQueueSize() == current_pbft_queue_size &&
        node2->getVoteQueueSize() == current_pbft_queue_size) {
      break;
    }
    taraxa::thisThreadSleepForMilliSeconds(100);
  }
  EXPECT_EQ(node1->getVoteQueueSize(), current_pbft_queue_size);
  EXPECT_EQ(node2->getVoteQueueSize(), current_pbft_queue_size);

  // period 1, step 5 TODO: need debug
  // node1 next vote for the pbft anchor block, 1 propose vote 3 soft votes 3
  // cert votes, 4 next votes
  pbft_mgr1->setPbftStep(5);
  pbft_mgr1->setPbftPeriod(1);
  pbft_mgr1->start();

  for (int i = 0; i < 3000; i++) {
    // test timeout is 3 seconds
    if (pbft_mgr1->getPbftStep() == 1 && pbft_mgr1->getPbftPeriod() == 2) {
      break;
    }
    taraxa::thisThreadSleepForMilliSeconds(1);
  }
  pbft_mgr1->stop();
  current_pbft_queue_size = 11;
  EXPECT_EQ(node1->getVoteQueueSize(), current_pbft_queue_size);

  for (int i = 0; i < 300; i++) {
    // test timeout is 30 seconds
    if (node2->getVoteQueueSize() == current_pbft_queue_size &&
        node3->getVoteQueueSize() == current_pbft_queue_size) {
      break;
    }
    taraxa::thisThreadSleepForMilliSeconds(100);
  }
  EXPECT_EQ(node2->getVoteQueueSize(), current_pbft_queue_size);
  EXPECT_EQ(node3->getVoteQueueSize(), current_pbft_queue_size);
  // node2 cert vote for the pbft anchor block, 1 propose vote 3 soft votes 3
  // cert votes, 5 next votes
  pbft_mgr2->setPbftStep(5);
  pbft_mgr2->setPbftPeriod(1);
  pbft_mgr2->start();

  for (int i = 0; i < 3000; i++) {
    // test timeout is 3 seconds
    if (pbft_mgr2->getPbftStep() == 1 && pbft_mgr2->getPbftPeriod() == 2) {
      break;
    }
    taraxa::thisThreadSleepForMilliSeconds(1);
  }
  pbft_mgr2->stop();
  current_pbft_queue_size = 12;
  EXPECT_EQ(node2->getVoteQueueSize(), current_pbft_queue_size);

  for (int i = 0; i < 300; i++) {
    // test timeout is 30 seconds
    if (node1->getVoteQueueSize() == current_pbft_queue_size &&
        node3->getVoteQueueSize() == current_pbft_queue_size) {
      break;
    }
    taraxa::thisThreadSleepForMilliSeconds(100);
  }
  EXPECT_EQ(node1->getVoteQueueSize(), current_pbft_queue_size);
  EXPECT_EQ(node3->getVoteQueueSize(), current_pbft_queue_size);
  // node3 cert vote for the pbft anchor block, 1 propose vote 3 soft votes 3
  // cert votes, 6 next votes
  pbft_mgr3->setPbftStep(5);
  pbft_mgr3->setPbftPeriod(1);
  pbft_mgr3->start();

  for (int i = 0; i < 3000; i++) {
    // test timeout is 3 seconds
    if (pbft_mgr3->getPbftStep() == 1 && pbft_mgr3->getPbftPeriod() == 2) {
      break;
    }
    taraxa::thisThreadSleepForMilliSeconds(1);
  }
  pbft_mgr3->stop();
  current_pbft_queue_size = 13;
  EXPECT_EQ(node3->getVoteQueueSize(), current_pbft_queue_size);

  for (int i = 0; i < 300; i++) {
    // test timeout is 30 seconds
    if (node1->getVoteQueueSize() == current_pbft_queue_size &&
        node2->getVoteQueueSize() == current_pbft_queue_size) {
      break;
    }
    taraxa::thisThreadSleepForMilliSeconds(100);
  }
  EXPECT_EQ(node1->getVoteQueueSize(), current_pbft_queue_size);
  EXPECT_EQ(node2->getVoteQueueSize(), current_pbft_queue_size);
  // TODO: need more test

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