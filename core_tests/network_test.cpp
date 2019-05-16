/*
 * @Copyright: Taraxa.io
 * @Author: Chia-Chun Lin
 * @Date: 2019-01-28 11:12:22
 * @Last Modified by: Chia-Chun Lin
 * @Last Modified time: 2019-04-23 18:31:04
 */

#include "network.hpp"
#include <gtest/gtest.h>
#include <atomic>
#include <boost/thread.hpp>
#include <iostream>
#include <vector>
#include "create_samples.hpp"
#include "libdevcore/DBFactory.h"
#include "libdevcore/Log.h"

namespace taraxa {

const unsigned NUM_TRX = 9;
auto g_secret = dev::Secret(
    "3800b2875669d9b2053c1aff9224ecfdc411423aac5b5a73d7a45ced1c3b9dcd",
    dev::Secret::ConstructFromStringType::FromHex);
auto g_trx_samples = samples::createMockTrxSamples(0, NUM_TRX);
auto g_signed_trx_samples =
    samples::createSignedTrxSamples(0, NUM_TRX, g_secret);

const unsigned NUM_TRX2 = 200;
auto g_secret2 = dev::Secret(
    "3800b2875669d9b2053c1aff9224ecfdc411423aac5b5a73d7a45ced1c3b9dcd",
    dev::Secret::ConstructFromStringType::FromHex);
auto g_trx_samples2 = samples::createMockTrxSamples(0, NUM_TRX2);
auto g_signed_trx_samples2 =
    samples::createSignedTrxSamples(0, NUM_TRX2, g_secret2);

FullNodeConfig g_conf1("./core_tests/conf_taraxa1.json");
FullNodeConfig g_conf2("./core_tests/conf_taraxa2.json");
FullNodeConfig g_conf3("./core_tests/conf_taraxa3.json");

/*
Test creates two Network setup and verifies sending block
between is successfull
*/
TEST(Network, transfer_block) {
  std::shared_ptr<Network> nw1(new taraxa::Network(g_conf1.network));
  std::shared_ptr<Network> nw2(new taraxa::Network(g_conf2.network));

  nw1->start();
  nw2->start();
  DagBlock blk(
      blk_hash_t(1111), {blk_hash_t(222), blk_hash_t(333), blk_hash_t(444)},
      {g_signed_trx_samples[0].getHash(), g_signed_trx_samples[1].getHash()},
      sig_t(7777), blk_hash_t(888), addr_t(999));

  std::unordered_map<trx_hash_t, Transaction> transactions;
  transactions[g_signed_trx_samples[0].getHash()] = g_signed_trx_samples[0];
  transactions[g_signed_trx_samples[1].getHash()] = g_signed_trx_samples[1];
  nw2->onNewTransactions(transactions);

  taraxa::thisThreadSleepForSeconds(1);

  for (auto i = 0; i < 1; ++i) {
    nw2->sendBlock(nw1->getNodeId(), blk, true);
  }

  std::cout << "Waiting packages for 10 seconds ..." << std::endl;

  for (int i = 0; i < 10; i++) {
    if (nw1->getReceivedBlocksCount()) break;
    taraxa::thisThreadSleepForSeconds(1);
  }
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
  std::shared_ptr<Network> nw1(new taraxa::Network(g_conf1.network));
  std::shared_ptr<Network> nw2(new taraxa::Network(g_conf2.network));

  nw1->start(true);
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

  taraxa::thisThreadSleepForSeconds(1);

  nw2->sendTransactions(nw1->getNodeId(), transactions);

  std::cout << "Waiting packages for 10 seconds ..." << std::endl;

  for (int i = 0; i < 10; i++) {
    if (nw1->getReceivedTransactionsCount()) break;
    taraxa::thisThreadSleepForSeconds(1);
  }

  nw2->stop();
  nw1->stop();
  unsigned long long num_received = nw1->getReceivedTransactionsCount();
  ASSERT_EQ(3, num_received);
}

/*
Test verifies saving network to a file and restoring it from a file
is successfull. Once restored from the file it is able to reestablish
connections even with boot nodes down
*/
TEST(Network, save_network) {
  {
    std::shared_ptr<Network> nw1(new taraxa::Network(g_conf1.network));
    std::shared_ptr<Network> nw2(new taraxa::Network(g_conf2.network));
    std::shared_ptr<Network> nw3(new taraxa::Network(g_conf3.network));

    nw1->start(true);
    nw2->start();
    nw3->start();

    for (int i = 0; i < 30; i++) {
      taraxa::thisThreadSleepForSeconds(1);
      if (2 == nw1->getPeerCount() && 2 == nw2->getPeerCount() &&
          2 == nw3->getPeerCount())
        break;
    }

    ASSERT_EQ(2, nw1->getPeerCount());
    ASSERT_EQ(2, nw2->getPeerCount());
    ASSERT_EQ(2, nw3->getPeerCount());

    nw1->stop();
    nw2->stop();
    nw3->stop();
    taraxa::thisThreadSleepForSeconds(1);
    nw2->saveNetwork("/tmp/nw2");
    nw3->saveNetwork("/tmp/nw3");
  }

  std::shared_ptr<Network> nw2(
      new taraxa::Network(g_conf2.network, "/tmp/nw2"));
  std::shared_ptr<Network> nw3(
      new taraxa::Network(g_conf3.network, "/tmp/nw3"));
  nw2->start();
  nw3->start();

  for (int i = 0; i < 20; i++) {
    taraxa::thisThreadSleepForSeconds(1);
    if (1 == nw2->getPeerCount() && 1 == nw3->getPeerCount()) break;
  }

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
      context1, std::string("./core_tests/conf_taraxa1.json")));

  node1->setDebug(true);
  node1->start(true);

  // Allow node to start up
  taraxa::thisThreadSleepForMilliSeconds(1000);

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

  taraxa::thisThreadSleepForMilliSeconds(1000);

  auto node2 = std::make_shared<taraxa::FullNode>(
      context2, std::string("./core_tests/conf_taraxa2.json"));

  node2->setDebug(true);
  node2->start(false /*boot_node*/);

  std::cout << "Waiting Sync for max 20000 milliseconds ..." << std::endl;
  for (int i = 0; i < 20; i++) {
    taraxa::thisThreadSleepForMilliSeconds(1000);
    if (node2->getNumVerticesInDag().first == 7 &&
        node2->getNumEdgesInDag().first == 8)
      break;
  }
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
Test creates a PBFT chain on one node and verifies
that the second node syncs with it and that the resulting
chain on the other end is the same
*/
TEST(Network, node_pbft_sync) {
  boost::asio::io_context context1;
  boost::asio::io_context context2;

  auto node1(std::make_shared<taraxa::FullNode>(
      context1, std::string("./core_tests/conf_taraxa1.json")));

  node1->setDebug(true);
  node1->start(true);

  // Allow node to start up
  taraxa::thisThreadSleepForMilliSeconds(1000);

  std::vector<PbftBlock> blks;

  blk_hash_t prev_pivot_blk(0);
  blk_hash_t prev_res_blk(0);
  blk_hash_t dag_blk(78);
  uint64_t epoch = 1;
  uint64_t timestamp1 = 123456;
  addr_t beneficiary(10);

  PivotBlock pivot_block(prev_pivot_blk, prev_res_blk, dag_blk, epoch,
                         timestamp1, beneficiary);
  PbftBlock pbft_block1(blk_hash_t(1));
  pbft_block1.setPivotBlock(pivot_block);

  uint64_t timestamp2 = 333333;
  vec_blk_t blocks{blk_hash_t(123), blk_hash_t(456), blk_hash_t(789)};
  std::vector<std::vector<uint>> modes{
      {0, 1, 2, 0, 1, 2}, {1, 1, 1, 1, 1}, {0, 0, 0}};
  TrxSchedule schedule(blocks, modes);
  blk_hash_t prev_pivot(1);
  ScheduleBlock schedule_blk(prev_pivot, timestamp2, schedule);

  PbftBlock pbft_block2(blk_hash_t(2));
  pbft_block2.setScheduleBlock(schedule_blk);

  blks.push_back(pbft_block1);
  blks.push_back(pbft_block2);

  for (auto i = 0; i < blks.size(); ++i) {
    node1->setPbftBlock(blks[i]);
  }

  taraxa::thisThreadSleepForMilliSeconds(1000);

  auto node2 = std::make_shared<taraxa::FullNode>(
      context2, std::string("./core_tests/conf_taraxa2.json"));

  node2->setDebug(true);
  node2->start(false /*boot_node*/);

  std::cout << "Waiting Sync for max 20000 milliseconds ..." << std::endl;
  for (int i = 0; i < 20; i++) {
    taraxa::thisThreadSleepForMilliSeconds(1000);
    if (node2->getPbftChainSize() == 3) break;
  }
  node1->stop();
  node2->stop();

  EXPECT_EQ(node1->getPbftChainSize(), 3);
  EXPECT_EQ(node2->getPbftChainSize(), 3);
}

/*
Test creates a DAG on one node and verifies
that the second node syncs with it and that the resulting
DAG on the other end is the same
Unlike the previous tests, this DAG contains blocks with transactions
and verifies that the sync containing transactions is successful
*/
TEST(Network, node_sync_with_transactions) {
  boost::asio::io_context context1;
  boost::asio::io_context context2;

  auto node1(std::make_shared<taraxa::FullNode>(
      context1, std::string("./core_tests/conf_taraxa1.json")));

  node1->setDebug(true);
  node1->start(true);
  std::vector<DagBlock> blks;

  DagBlock blk1(
      blk_hash_t(0), {},
      {g_signed_trx_samples[0].getHash(), g_signed_trx_samples[1].getHash()},
      sig_t(777), blk_hash_t(1), addr_t(999));
  std::vector<Transaction> tr1(
      {g_signed_trx_samples[0], g_signed_trx_samples[1]});

  DagBlock blk2(blk_hash_t(01), {}, {g_signed_trx_samples[2].getHash()},
                sig_t(777), blk_hash_t(02), addr_t(999));
  std::vector<Transaction> tr2({g_signed_trx_samples[2]});

  DagBlock blk3(blk_hash_t(02), {}, {}, sig_t(777), blk_hash_t(03),
                addr_t(999));
  std::vector<Transaction> tr3;

  DagBlock blk4(
      blk_hash_t(03), {},
      {g_signed_trx_samples[3].getHash(), g_signed_trx_samples[4].getHash()},
      sig_t(777), blk_hash_t(04), addr_t(999));
  std::vector<Transaction> tr4(
      {g_signed_trx_samples[3], g_signed_trx_samples[4]});

  DagBlock blk5(
      blk_hash_t(04), {},
      {g_signed_trx_samples[5].getHash(), g_signed_trx_samples[6].getHash(),
       g_signed_trx_samples[7].getHash(), g_signed_trx_samples[8].getHash()},
      sig_t(777), blk_hash_t(05), addr_t(999));
  std::vector<Transaction> tr5(
      {g_signed_trx_samples[5], g_signed_trx_samples[6],
       g_signed_trx_samples[7], g_signed_trx_samples[8]});

  DagBlock blk6(blk_hash_t(05), {blk_hash_t(04), blk_hash_t(3)}, {}, sig_t(777),
                blk_hash_t(06), addr_t(999));
  std::vector<Transaction> tr6;

  node1->storeBlockWithTransactions(blk6, tr6);
  node1->storeBlockWithTransactions(blk5, tr5);
  node1->storeBlockWithTransactions(blk4, tr4);
  node1->storeBlockWithTransactions(blk3, tr3);
  node1->storeBlockWithTransactions(blk2, tr2);
  node1->storeBlockWithTransactions(blk1, tr1);

  // To make sure blocks are stored before starting node 2
  taraxa::thisThreadSleepForMilliSeconds(1000);

  auto node2 = std::make_shared<taraxa::FullNode>(
      context2, std::string("./core_tests/conf_taraxa2.json"));

  node2->setDebug(true);
  node2->start(false /*boot_node*/);

  std::cout << "Waiting Sync for up to 20000 milliseconds ..." << std::endl;
  for (int i = 0; i < 20; i++) {
    taraxa::thisThreadSleepForMilliSeconds(1000);
    if (node2->getNumVerticesInDag().first == 7 &&
        node2->getNumEdgesInDag().first == 8)
      break;
  }

  node1->stop();
  node2->stop();

  // node1->drawGraph("dot.txt");
  EXPECT_EQ(node1->getNumReceivedBlocks(), 6);
  EXPECT_EQ(node1->getNumVerticesInDag().first, 7);
  EXPECT_EQ(node1->getNumEdgesInDag().first, 8);

  EXPECT_EQ(node2->getNumReceivedBlocks(), 6);
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
      context1, std::string("./core_tests/conf_taraxa1.json")));

  node1->setDebug(true);
  node1->start(true);
  std::vector<DagBlock> blks;

  DagBlock blk1(
      blk_hash_t(0), {},
      {g_signed_trx_samples2[0].getHash(), g_signed_trx_samples2[1].getHash()},
      sig_t(777), blk_hash_t(0xB1), addr_t(999));
  std::vector<Transaction> tr1(
      {g_signed_trx_samples2[0], g_signed_trx_samples2[1]});

  DagBlock blk2(
      blk_hash_t(0), {},
      {g_signed_trx_samples2[2].getHash(), g_signed_trx_samples2[3].getHash()},
      sig_t(777), blk_hash_t(0xB2), addr_t(999));
  std::vector<Transaction> tr2(
      {g_signed_trx_samples2[2], g_signed_trx_samples2[3]});

  DagBlock blk3(
      blk_hash_t(0xB1), {},
      {g_signed_trx_samples2[4].getHash(), g_signed_trx_samples2[5].getHash()},
      sig_t(777), blk_hash_t(0xB6), addr_t(999));
  std::vector<Transaction> tr3(
      {g_signed_trx_samples2[4], g_signed_trx_samples2[5]});

  DagBlock blk4(
      blk_hash_t(0xB6), {},
      {g_signed_trx_samples2[6].getHash(), g_signed_trx_samples2[7].getHash()},
      sig_t(777), blk_hash_t(0xB10), addr_t(999));
  std::vector<Transaction> tr4(
      {g_signed_trx_samples2[6], g_signed_trx_samples2[7]});

  DagBlock blk5(
      blk_hash_t(0xB2), {},
      {g_signed_trx_samples2[8].getHash(), g_signed_trx_samples2[9].getHash()},
      sig_t(777), blk_hash_t(0xB8), addr_t(999));
  std::vector<Transaction> tr5(
      {g_signed_trx_samples2[8], g_signed_trx_samples2[9]});

  DagBlock blk6(blk_hash_t(0xB1), {},
                {g_signed_trx_samples2[10].getHash(),
                 g_signed_trx_samples2[11].getHash()},
                sig_t(777), blk_hash_t(0xB3), addr_t(999));
  std::vector<Transaction> tr6(
      {g_signed_trx_samples2[10], g_signed_trx_samples2[11]});

  DagBlock blk7(blk_hash_t(0xB3), {},
                {g_signed_trx_samples2[12].getHash(),
                 g_signed_trx_samples2[13].getHash()},
                sig_t(777), blk_hash_t(0xB4), addr_t(999));
  std::vector<Transaction> tr7(
      {g_signed_trx_samples2[12], g_signed_trx_samples2[13]});

  DagBlock blk8(blk_hash_t(0xB1), {blk_hash_t(0xB4)},
                {g_signed_trx_samples2[14].getHash(),
                 g_signed_trx_samples2[15].getHash()},
                sig_t(777), blk_hash_t(0xB5), addr_t(999));
  std::vector<Transaction> tr8(
      {g_signed_trx_samples2[14], g_signed_trx_samples2[15]});

  DagBlock blk9(blk_hash_t(0xB1), {},
                {g_signed_trx_samples2[16].getHash(),
                 g_signed_trx_samples2[17].getHash()},
                sig_t(777), blk_hash_t(0xB7), addr_t(999));
  std::vector<Transaction> tr9(
      {g_signed_trx_samples2[16], g_signed_trx_samples2[17]});

  DagBlock blk10(blk_hash_t(0xB5), {},
                 {g_signed_trx_samples2[18].getHash(),
                  g_signed_trx_samples2[19].getHash()},
                 sig_t(777), blk_hash_t(0xB9), addr_t(999));
  std::vector<Transaction> tr10(
      {g_signed_trx_samples2[18], g_signed_trx_samples2[19]});

  DagBlock blk11(blk_hash_t(0xB6), {},
                 {g_signed_trx_samples2[20].getHash(),
                  g_signed_trx_samples2[21].getHash()},
                 sig_t(777), blk_hash_t(0xB11), addr_t(999));
  std::vector<Transaction> tr11(
      {g_signed_trx_samples2[20], g_signed_trx_samples2[21]});

  DagBlock blk12(blk_hash_t(0xB8), {},
                 {g_signed_trx_samples2[22].getHash(),
                  g_signed_trx_samples2[23].getHash()},
                 sig_t(777), blk_hash_t(0xB12), addr_t(999));
  std::vector<Transaction> tr12(
      {g_signed_trx_samples2[22], g_signed_trx_samples2[23]});

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

  std::vector<std::vector<Transaction>> trxs;
  trxs.push_back(tr1);
  trxs.push_back(tr2);
  trxs.push_back(tr3);
  trxs.push_back(tr4);
  trxs.push_back(tr5);
  trxs.push_back(tr6);
  trxs.push_back(tr7);
  trxs.push_back(tr8);
  trxs.push_back(tr9);
  trxs.push_back(tr10);
  trxs.push_back(tr11);
  trxs.push_back(tr12);

  for (auto i = 0; i < blks.size(); ++i) {
    node1->storeBlockWithTransactions(blks[i], trxs[i]);
  }

  taraxa::thisThreadSleepForMilliSeconds(2000);

  auto node2(std::make_shared<taraxa::FullNode>(
      context2, std::string("./core_tests/conf_taraxa2.json")));

  node2->setDebug(true);
  node2->start(false /*boot_node*/);

  std::cout << "Waiting Sync for up to 20000 milliseconds ..." << std::endl;
  for (int i = 0; i < 20; i++) {
    taraxa::thisThreadSleepForMilliSeconds(1000);
    if (node2->getNumVerticesInDag().first == 13 &&
        node2->getNumEdgesInDag().first == 13)
      break;
  }

  // node1->drawGraph("dot.txt");
  EXPECT_EQ(node1->getNumReceivedBlocks(), blks.size());
  EXPECT_EQ(node1->getNumVerticesInDag().first, 13);
  EXPECT_EQ(node1->getNumEdgesInDag().first, 13);

  EXPECT_EQ(node2->getNumReceivedBlocks(), blks.size());
  EXPECT_EQ(node2->getNumVerticesInDag().first, 13);
  EXPECT_EQ(node2->getNumEdgesInDag().first, 13);

  node1->stop();
  node2->stop();

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
      context1, std::string("./core_tests/conf_taraxa1.json")));

  node1->setDebug(true);
  node1->start(true);

  std::unordered_map<trx_hash_t, Transaction> transactions;
  for (auto const& t : g_signed_trx_samples) {
    transactions[t.getHash()] = t;
  }

  node1->insertNewTransactions(transactions);

  taraxa::thisThreadSleepForMilliSeconds(1000);

  auto node2 = std::make_shared<taraxa::FullNode>(
      context2, std::string("./core_tests/conf_taraxa2.json"));

  node2->setDebug(true);
  node2->start(false /*boot_node*/);

  std::cout << "Waiting Sync for 2000 milliseconds ..." << std::endl;
  taraxa::thisThreadSleepForMilliSeconds(2000);

  node1->stop();
  node2->stop();

  for (auto const& t : g_signed_trx_samples) {
    EXPECT_TRUE(node2->getTransaction(t.getHash()) != nullptr);
    if (node2->getTransaction(t.getHash()) != nullptr) {
      EXPECT_EQ(t, *node2->getTransaction(t.getHash()));
    }
  }
}

/*
Test creates multiple nodes and creates new transactions in random time
intervals on randomly selected nodes It verifies that the blocks created from
these transactions which get created on random nodes are synced and the
resulting DAG is the same on all nodes
*/
TEST(Network, node_full_sync) {
  const int numberOfNodes = 5;
  boost::asio::io_context context1;
  std::vector<std::shared_ptr<boost::asio::io_context>> contexts;

  auto node1(std::make_shared<taraxa::FullNode>(
      context1, std::string("./core_tests/conf_taraxa1.json")));

  node1->setDebug(true);
  node1->start(true /*boot_node*/);

  std::vector<std::shared_ptr<FullNode>> nodes;
  for (int i = 0; i < numberOfNodes; i++) {
    contexts.push_back(std::make_shared<boost::asio::io_context>());
    FullNodeConfig config(std::string("./core_tests/conf_taraxa2.json"));
    config.db_accounts_path += std::to_string(i + 1);
    config.db_blocks_path += std::to_string(i + 1);
    config.db_transactions_path += std::to_string(i + 1);
    config.network.network_listen_port += i + 1;
    config.node_secret = "";
    nodes.push_back(std::make_shared<taraxa::FullNode>(*contexts[i], config));
    nodes[i]->start(false /*boot_node*/);
    taraxa::thisThreadSleepForMilliSeconds(50);
  }

  taraxa::thisThreadSleepForMilliSeconds(10000);

  std::random_device dev;
  std::mt19937 rng(dev());
  std::uniform_int_distribution<std::mt19937::result_type> distTransactions(
      1, 200);
  std::uniform_int_distribution<std::mt19937::result_type> distNodes(
      0, numberOfNodes - 1);  // distribution in range [1, 2000]

  int counter = 0;
  for (auto const& t : g_signed_trx_samples2) {
    std::unordered_map<trx_hash_t, Transaction> transactions;
    transactions[t.getHash()] = t;
    nodes[distNodes(rng)]->insertNewTransactions(transactions);
    thisThreadSleepForMilliSeconds(distTransactions(rng));
    counter++;
    // printf("Created transaction %d, vertices %lu\n", counter,
    // node1->getNumVerticesInDag().first);
  }

  std::cout << "Waiting Sync for up to 2 minutes ..." << std::endl;
  for (int i = 0; i < 24; i++) {
    taraxa::thisThreadSleepForMilliSeconds(5000);
    bool finished = true;
    for (int j = 0; j < numberOfNodes; j++) {
      if (nodes[j]->getNumVerticesInDag().first !=
          node1->getNumVerticesInDag().first) {
        finished = false;
        break;
      }
    }
    if (finished) break;
  }
  // printf("End result: Vertices %lu Edges: %lu \n",
  // node1->getNumVerticesInDag().first, node1->getNumEdgesInDag().first);

  node1->stop();
  for (int i = 0; i < numberOfNodes; i++) nodes[i]->stop();

  // node1->drawGraph("node1.txt");
  // for (int i = 0; i < numberOfNodes; i++)
  //   nodes[i]->drawGraph(std::string("node") + std::to_string(i + 2) +
  //   ".txt");

  EXPECT_GT(node1->getNumVerticesInDag().first, 0);
  EXPECT_GT(10, node1->getNewVerifiedTrxSnapShot(false).size());
  for (int i = 0; i < numberOfNodes; i++) {
    EXPECT_GT(nodes[i]->getNumVerticesInDag().first, 0);
    EXPECT_EQ(nodes[i]->getNumVerticesInDag().first,
              node1->getNumVerticesInDag().first);
    EXPECT_EQ(nodes[i]->getNumEdgesInDag().first,
              node1->getNumEdgesInDag().first);
  }
}

}  // namespace taraxa

int main(int argc, char** argv) {
  dev::LoggingOptions logOptions;
  logOptions.verbosity = dev::VerbosityWarning;
  logOptions.includeChannels.push_back("NETWORK");
  logOptions.includeChannels.push_back("TARCAP");
  dev::setupLogging(logOptions);
  // use the in-memory db so test will not affect other each other through
  // persistent storage
  dev::db::setDatabaseKind(dev::db::DatabaseKind::MemoryDB);
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}