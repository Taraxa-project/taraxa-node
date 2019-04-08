/*
 * @Copyright: Taraxa.io
 * @Author: Chia-Chun Lin
 * @Date: 2019-01-28 11:12:22
 * @Last Modified by: Chia-Chun Lin
 * @Last Modified time: 2019-03-15 21:24:34
 */

#include "network.hpp"
#include <gtest/gtest.h>
#include <atomic>
#include <boost/thread.hpp>
#include <iostream>
#include <vector>
#include "create_samples.hpp"
#include "libdevcore/Log.h"

namespace taraxa {

const unsigned NUM_TRX = 9;
auto g_secret = dev::Secret(
    "3800b2875669d9b2053c1aff9224ecfdc411423aac5b5a73d7a45ced1c3b9dcd",
    dev::Secret::ConstructFromStringType::FromHex);
auto g_trx_samples = samples::createMockTrxSamples(0, NUM_TRX);
auto g_signed_trx_samples =
    samples::createSignedTrxSamples(0, NUM_TRX, g_secret);

const unsigned NUM_TRX2 = 50;
auto g_secret2 = dev::Secret(
    "3800b2875669d9b2053c1aff9224ecfdc411423aac5b5a73d7a45ced1c3b9dcd",
    dev::Secret::ConstructFromStringType::FromHex);
auto g_trx_samples2 = samples::createMockTrxSamples(0, NUM_TRX);
auto g_signed_trx_samples2 =
    samples::createSignedTrxSamples(0, NUM_TRX2, g_secret2);

/*
Test creates two Network setup and verifies sending block
between is successfull
*/
TEST(Network, transfer_block) {
  std::shared_ptr<Network> nw1(
      new taraxa::Network("./core_tests/conf_network1.json"));
  std::shared_ptr<Network> nw2(
      new taraxa::Network("./core_tests/conf_network2.json"));

  nw1->start();
  nw2->start();
  DagBlock blk(blk_hash_t(1111),
               {blk_hash_t(222), blk_hash_t(333), blk_hash_t(444)},
               {trx_hash_t(555), trx_hash_t(666)}, sig_t(7777), blk_hash_t(888),
               addr_t(999));

  taraxa::thisThreadSleepForSeconds(10);

  for (auto i = 0; i < 1; ++i) {
    nw2->sendBlock(nw1->getNodeId(), blk, true);
  }

  std::cout << "Waiting packages for 10 seconds ..." << std::endl;

  taraxa::thisThreadSleepForSeconds(10);

  nw2->stop();
  nw1->stop();
  unsigned long long num_received = nw1->getReceivedBlocksCount();
  ASSERT_EQ(1, num_received);
}

/*
Test creates two Network setup and verifies sending transaction
between is successfull
*/
TEST(Network, transfer_transaction) {
  std::shared_ptr<Network> nw1(
      new taraxa::Network("./core_tests/conf_network1.json"));
  std::shared_ptr<Network> nw2(
      new taraxa::Network("./core_tests/conf_network2.json"));

  nw1->start();
  nw2->start();
  Transaction trx1(trx_hash_t(1),            // hash
                   Transaction::Type::Call,  // type
                   2,                        // nonce
                   3,                        // value
                   val_t(4),                 // gas_price
                   val_t(5),                 // gas
                   addr_t(1000),             // receiver
                   sig_t(),                  // sig
                   str2bytes("00FEDCBA9876543210000000"));
  Transaction trx2(trx_hash_t(2),            // hash
                   Transaction::Type::Call,  // type
                   2,                        // nonce
                   3,                        // value
                   val_t(4),                 // gas_price
                   val_t(5),                 // gas
                   addr_t(1000),             // receiver
                   sig_t(),                  // sig
                   str2bytes("00FEDCBA9876543210000000"));
  Transaction trx3(trx_hash_t(3),            // hash
                   Transaction::Type::Call,  // type
                   2,                        // nonce
                   3,                        // value
                   val_t(4),                 // gas_price
                   val_t(5),                 // gas
                   addr_t(1000),             // receiver
                   sig_t(),                  // sig
                   str2bytes("00FEDCBA9876543210000000"));

  std::vector<Transaction> transactions;
  transactions.push_back(trx1);
  transactions.push_back(trx2);
  transactions.push_back(trx3);

  taraxa::thisThreadSleepForSeconds(10);

  nw2->sendTransactions(nw1->getNodeId(), transactions);

  std::cout << "Waiting packages for 10 seconds ..." << std::endl;

  taraxa::thisThreadSleepForSeconds(10);

  nw2->stop();
  nw1->stop();
  unsigned long long num_received = nw1->getReceivedTransactionsCount();
  ASSERT_EQ(3, num_received);
}

/*
Test verifies saving network to a file and restoring it rom a file
is successfull. Once restored from the file it is able to reestablish connections
even with boot nodes down
*/
TEST(Network, save_network) {
  {
    std::shared_ptr<Network> nw1(
        new taraxa::Network("./core_tests/conf_network1.json"));
    std::shared_ptr<Network> nw2(
        new taraxa::Network("./core_tests/conf_network2.json"));
    std::shared_ptr<Network> nw3(
        new taraxa::Network("./core_tests/conf_network3.json"));

    nw1->start();
    nw2->start();
    nw3->start();

    taraxa::thisThreadSleepForSeconds(20);

    ASSERT_EQ(2, nw1->getPeerCount());
    ASSERT_EQ(2, nw2->getPeerCount());
    ASSERT_EQ(2, nw3->getPeerCount());

    nw1->stop();
    nw2->stop();
    nw3->stop();
    taraxa::thisThreadSleepForSeconds(10);
    nw2->saveNetwork("/tmp/nw2");
    nw3->saveNetwork("/tmp/nw3");
  }

  std::shared_ptr<Network> nw2(
      new taraxa::Network("./core_tests/conf_network2.json", "/tmp/nw2"));
  std::shared_ptr<Network> nw3(
      new taraxa::Network("./core_tests/conf_network3.json", "/tmp/nw3"));
  nw2->start();
  nw3->start();

  taraxa::thisThreadSleepForSeconds(10);

  ASSERT_EQ(1, nw2->getPeerCount());
  ASSERT_EQ(1, nw3->getPeerCount());
}

/*
Test creates a DAG on one node and verifies
that the second node syncs with it and that the resulting
DAG on the other end is the same
*/
TEST(Network, node_sync) {
  boost::asio::io_context context1;
  boost::asio::io_context context2;

  auto node1(std::make_shared<taraxa::FullNode>(
      context1, std::string("./core_tests/conf_full_node1.json"),
      std::string("./core_tests/conf_network1.json")));

  node1->setDebug(true);
  node1->start();
  std::vector<DagBlock> blks;

  DagBlock blk1(blk_hash_t(0), {}, {}, sig_t(777), blk_hash_t(1), addr_t(999));

  DagBlock blk2(blk_hash_t(01), {}, {}, sig_t(777), blk_hash_t(02),
                addr_t(999));

  DagBlock blk3(blk_hash_t(02), {}, {}, sig_t(777), blk_hash_t(03),
                addr_t(999));

  DagBlock blk4(blk_hash_t(03), {}, {}, sig_t(777), blk_hash_t(04),
                addr_t(999));

  DagBlock blk5(blk_hash_t(04), {}, {}, sig_t(777), blk_hash_t(05),
                addr_t(999));

  DagBlock blk6(blk_hash_t(05), {blk_hash_t(04), blk_hash_t(3)}, {}, sig_t(777),
                blk_hash_t(06), addr_t(999));

  blks.push_back(blk6);
  blks.push_back(blk5);
  blks.push_back(blk4);
  blks.push_back(blk3);
  blks.push_back(blk2);
  blks.push_back(blk1);

  for (auto i = 0; i < blks.size(); ++i) {
    node1->storeBlock(blks[i]);
  }

  auto node2 = std::make_shared<taraxa::FullNode>(
      context2, std::string("./core_tests/conf_full_node2.json"),
      std::string("./core_tests/conf_network2.json"));

  node2->setDebug(true);
  node2->start();

  std::cout << "Waiting Sync for 5000 milliseconds ..." << std::endl;
  taraxa::thisThreadSleepForMilliSeconds(5000);

  node1->stop();
  node2->stop();

  // node1->drawGraph("dot.txt");
  EXPECT_EQ(node1->getNumReceivedBlocks(), blks.size());
  EXPECT_EQ(node1->getNumVerticesInDag().first, 7);
  EXPECT_EQ(node1->getNumEdgesInDag().first, 8);

  EXPECT_EQ(node2->getNumReceivedBlocks(), blks.size());
  EXPECT_EQ(node2->getNumVerticesInDag().first, 7);
  EXPECT_EQ(node2->getNumEdgesInDag().first, 8);
}

/*
Test creates a complex DAG on one node and verifies
that the second node syncs with it and that the resulting
DAG on the other end is the same
*/
TEST(Network, node_sync2) {
  boost::asio::io_context context1;
  boost::asio::io_context context2;

  taraxa::thisThreadSleepForMilliSeconds(2000);

  auto node1(std::make_shared<taraxa::FullNode>(
      context1, std::string("./core_tests/conf_full_node1.json"),
      std::string("./core_tests/conf_network1.json")));

  node1->setDebug(true);
  node1->start();
  std::vector<DagBlock> blks;

  DagBlock blk1(blk_hash_t(0), {}, {}, sig_t(777), blk_hash_t(0xB1),
                addr_t(999));

  DagBlock blk2(blk_hash_t(0), {}, {}, sig_t(777), blk_hash_t(0xB2),
                addr_t(999));

  DagBlock blk3(blk_hash_t(0xB1), {}, {}, sig_t(777), blk_hash_t(0xB6),
                addr_t(999));

  DagBlock blk4(blk_hash_t(0xB6), {}, {}, sig_t(777), blk_hash_t(0xB10),
                addr_t(999));

  DagBlock blk5(blk_hash_t(0xB2), {}, {}, sig_t(777), blk_hash_t(0xB8),
                addr_t(999));

  DagBlock blk6(blk_hash_t(0xB1), {}, {}, sig_t(777), blk_hash_t(0xB3),
                addr_t(999));

  DagBlock blk7(blk_hash_t(0xB3), {}, {}, sig_t(777), blk_hash_t(0xB4),
                addr_t(999));

  DagBlock blk8(blk_hash_t(0xB1), {blk_hash_t(0xB4)}, {}, sig_t(777),
                blk_hash_t(0xB5), addr_t(999));

  DagBlock blk9(blk_hash_t(0xB1), {}, {}, sig_t(777), blk_hash_t(0xB7),
                addr_t(999));

  DagBlock blk10(blk_hash_t(0xB5), {}, {}, sig_t(777), blk_hash_t(0xB9),
                 addr_t(999));

  DagBlock blk11(blk_hash_t(0xB6), {}, {}, sig_t(777), blk_hash_t(0xB11),
                 addr_t(999));

  DagBlock blk12(blk_hash_t(0xB8), {}, {}, sig_t(777), blk_hash_t(0xB12),
                 addr_t(999));

  blks.push_back(blk1);
  blks.push_back(blk2);
  blks.push_back(blk3);
  blks.push_back(blk4);
  blks.push_back(blk5);
  blks.push_back(blk6);
  blks.push_back(blk7);
  blks.push_back(blk8);
  blks.push_back(blk9);
  blks.push_back(blk10);
  blks.push_back(blk11);
  blks.push_back(blk12);

  for (auto i = 0; i < blks.size(); ++i) {
    node1->storeBlock(blks[i]);
  }

  taraxa::thisThreadSleepForMilliSeconds(5000);

  auto node2(std::make_shared<taraxa::FullNode>(
      context2, std::string("./core_tests/conf_full_node2.json"),
      std::string("./core_tests/conf_network2.json")));

  node2->setDebug(true);
  node2->start();

  std::cout << "Waiting Sync for 5000 milliseconds ..." << std::endl;
  taraxa::thisThreadSleepForMilliSeconds(5000);

  node1->stop();
  node2->stop();

  // node1->drawGraph("dot.txt");
  EXPECT_EQ(node1->getNumReceivedBlocks(), blks.size());
  EXPECT_EQ(node1->getNumVerticesInDag().first, 13);
  EXPECT_EQ(node1->getNumEdgesInDag().first, 13);

  EXPECT_EQ(node2->getNumReceivedBlocks(), blks.size());
  EXPECT_EQ(node2->getNumVerticesInDag().first, 13);
  EXPECT_EQ(node2->getNumEdgesInDag().first, 13);

  node1.reset();
  node2.reset();
}

/*
Test creates new transactions on one node and verifies
that the second node receives the transactions
*/
TEST(Network, node_transaction_sync) {
  boost::asio::io_context context1;
  boost::asio::io_context context2;

  auto node1(std::make_shared<taraxa::FullNode>(
      context1, std::string("./core_tests/conf_full_node1.json"),
      std::string("./core_tests/conf_network1.json")));

  node1->setDebug(true);
  node1->start();

  std::unordered_map<trx_hash_t, Transaction> transactions;
  for (auto const& t : g_signed_trx_samples) {
    transactions[t.getHash()] = t;
  }

  node1->insertNewTransactions(transactions);

  auto node2 = std::make_shared<taraxa::FullNode>(
      context2, std::string("./core_tests/conf_full_node2.json"),
      std::string("./core_tests/conf_network2.json"));

  node2->setDebug(true);
  node2->start();

  std::cout << "Waiting Sync for 5000 milliseconds ..." << std::endl;
  taraxa::thisThreadSleepForMilliSeconds(5000);

  node1->stop();
  node2->stop();

  // node1->drawGraph("dot.txt");
  EXPECT_EQ(node1->getNewVerifiedTrxSnapShot(false).size(), g_signed_trx_samples.size());
  EXPECT_EQ(node2->getNewVerifiedTrxSnapShot(false).size(), g_signed_trx_samples.size());
}


/*
Test creates new transactions on one node and verifies
that the second node receives the transactions
*/
TEST(Network, node_full_sync) {
  const int numberOfNodes = 1;
  boost::asio::io_context context1;
  std::vector<std::shared_ptr<boost::asio::io_context> > contexts;
  
  auto node1(std::make_shared<taraxa::FullNode>(
      context1, std::string("./core_tests/conf_full_node1.json"),
      std::string("./core_tests/conf_network1.json")));

  node1->setDebug(true);
  node1->start();

  std::vector<std::shared_ptr<FullNode> > nodes;
  for(int i = 0; i < numberOfNodes; i++) {
    contexts.push_back(std::make_shared<boost::asio::io_context>());
    nodes.push_back(std::make_shared<taraxa::FullNode>(
      *contexts[i], std::string("./core_tests/conf_full_node2.json"),
      std::string("./core_tests/conf_network0.json"), i + 1));
    nodes[i]->start();
  }

  std::unordered_map<trx_hash_t, Transaction> transactions;
  for (auto const& t : g_signed_trx_samples2) {
    transactions[t.getHash()] = t;
  }

  node1->insertNewTransactions(transactions);

  std::cout << "Waiting Sync for 30000 milliseconds ..." << std::endl;
  taraxa::thisThreadSleepForMilliSeconds(30000);

  node1->stop();
  for(int i = 0; i < numberOfNodes; i++)
    nodes[i]->stop();

  node1->drawGraph("node1.txt");
  for(int i = 0; i < numberOfNodes; i++)
    node1->drawGraph("node2.txt");
  
  // node1->drawGraph("dot.txt");
  EXPECT_EQ(node1->getNewVerifiedTrxSnapShot(false).size(), g_signed_trx_samples.size());
  for(int i = 0; i < numberOfNodes; i++)
    EXPECT_EQ(nodes[i]->getNewVerifiedTrxSnapShot(false).size(), g_signed_trx_samples.size());
}

}  // namespace taraxa

int main(int argc, char** argv) {
  dev::LoggingOptions logOptions;
  logOptions.verbosity = dev::VerbosityTrace;
  logOptions.includeChannels.push_back("network");
  dev::setupLogging(logOptions);
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}