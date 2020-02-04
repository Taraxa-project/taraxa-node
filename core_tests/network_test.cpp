#include "network.hpp"

#include <gtest/gtest.h>
#include <libdevcore/DBFactory.h>
#include <libdevcore/Log.h>

#include <atomic>
#include <boost/thread.hpp>
#include <iostream>
#include <vector>

#include "core_tests/util.hpp"
#include "create_samples.hpp"
#include "static_init.hpp"
#include "util/lazy.hpp"

namespace taraxa {
using ::taraxa::util::lazy::Lazy;

const unsigned NUM_TRX = 9;
auto g_secret = Lazy([] {
  return dev::Secret(
      "3800b2875669d9b2053c1aff9224ecfdc411423aac5b5a73d7a45ced1c3b9dcd",
      dev::Secret::ConstructFromStringType::FromHex);
});
auto g_trx_samples =
    Lazy([] { return samples::createMockTrxSamples(0, NUM_TRX); });
auto g_signed_trx_samples =
    Lazy([] { return samples::createSignedTrxSamples(0, NUM_TRX, g_secret); });

const unsigned NUM_TRX2 = 400;
auto g_secret2 = Lazy([] {
  return dev::Secret(
      "3800b2875669d9b2053c1aff9224ecfdc411423aac5b5a73d7a45ced1c3b9dcd",
      dev::Secret::ConstructFromStringType::FromHex);
});
auto g_trx_samples2 =
    Lazy([] { return samples::createMockTrxSamples(0, NUM_TRX2); });
auto g_signed_trx_samples2 = Lazy(
    [] { return samples::createSignedTrxSamples(0, NUM_TRX2, g_secret2); });
auto g_conf1 =
    Lazy([] { return FullNodeConfig("./core_tests/conf/conf_taraxa1.json"); });
auto g_conf2 =
    Lazy([] { return FullNodeConfig("./core_tests/conf/conf_taraxa2.json"); });
auto g_conf3 =
    Lazy([] { return FullNodeConfig("./core_tests/conf/conf_taraxa3.json"); });

struct NetworkTest : core_tests::util::DBUsingTest<> {};

/*
Test creates two Network setup and verifies sending block
between is successfull
*/
TEST_F(NetworkTest, transfer_block) {
  std::shared_ptr<Network> nw1(new taraxa::Network(
      g_conf1->network, g_conf1->chain.dag_genesis_block.getHash().toString()));
  std::shared_ptr<Network> nw2(new taraxa::Network(
      g_conf2->network, g_conf2->chain.dag_genesis_block.getHash().toString()));

  nw1->start();
  nw2->start();
  DagBlock blk(
      blk_hash_t(1111), 0, {blk_hash_t(222), blk_hash_t(333), blk_hash_t(444)},
      {g_signed_trx_samples[0].getHash(), g_signed_trx_samples[1].getHash()},
      sig_t(7777), blk_hash_t(888), addr_t(999));

  std::vector<taraxa::bytes> transactions;
  transactions.emplace_back(g_signed_trx_samples[0].rlp(true));
  transactions.emplace_back(g_signed_trx_samples[1].rlp(true));
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

TEST_F(NetworkTest, send_pbft_block) {
  std::shared_ptr<Network> nw1(new taraxa::Network(
      g_conf1->network, g_conf1->chain.dag_genesis_block.getHash().toString()));
  std::shared_ptr<Network> nw2(new taraxa::Network(
      g_conf2->network, g_conf2->chain.dag_genesis_block.getHash().toString()));

  nw1->start();
  nw2->start();
  PbftBlock pbft_block(blk_hash_t(1), 2);
  uint64_t chain_size = 111;
  taraxa::thisThreadSleepForSeconds(1);

  nw2->sendPbftBlock(nw1->getNodeId(), pbft_block, chain_size);
  taraxa::thisThreadSleepForMilliSeconds(200);

  ASSERT_EQ(1, nw1->getTaraxaCapability()->getAllPeers().size());
  ASSERT_EQ(chain_size,
            nw1->getTaraxaCapability()
                ->getPeer(nw1->getTaraxaCapability()->getAllPeers()[0])
                ->pbft_chain_size_);
  nw2->stop();
  nw1->stop();
}

/*
Test creates two Network setup and verifies sending transaction
between is successfull
*/
TEST_F(NetworkTest, transfer_transaction) {
  std::shared_ptr<Network> nw1(new taraxa::Network(
      g_conf1->network, g_conf1->chain.dag_genesis_block.getHash().toString()));
  std::shared_ptr<Network> nw2(new taraxa::Network(
      g_conf2->network, g_conf2->chain.dag_genesis_block.getHash().toString()));

  nw1->start(true);
  nw2->start();
  std::vector<taraxa::bytes> transactions;
  transactions.push_back(g_signed_trx_samples[0].rlp(true));
  transactions.push_back(g_signed_trx_samples[1].rlp(true));
  transactions.push_back(g_signed_trx_samples[2].rlp(true));

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
TEST_F(NetworkTest, save_network) {
  {
    std::shared_ptr<Network> nw1(new taraxa::Network(
        g_conf1->network,
        g_conf1->chain.dag_genesis_block.getHash().toString()));
    std::shared_ptr<Network> nw2(new taraxa::Network(
        g_conf2->network,
        g_conf2->chain.dag_genesis_block.getHash().toString()));
    std::shared_ptr<Network> nw3(new taraxa::Network(
        g_conf3->network,
        g_conf3->chain.dag_genesis_block.getHash().toString()));

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

  std::shared_ptr<Network> nw2(new taraxa::Network(
      g_conf2->network, "/tmp/nw2",
      g_conf2->chain.dag_genesis_block.getHash().toString()));
  std::shared_ptr<Network> nw3(new taraxa::Network(
      g_conf3->network, "/tmp/nw3",
      g_conf2->chain.dag_genesis_block.getHash().toString()));
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
Test creates one node with testnet network ID and one node with main ID and
verifies that connection fails
*/
TEST_F(NetworkTest, node_network_id) {
  {
    FullNodeConfig conf1(std::string("./core_tests/conf/conf_taraxa1.json"));
    conf1.network.network_id = "main";
    auto node1(taraxa::FullNode::make(conf1, true));

    node1->setDebug(true);
    node1->start(true);

    FullNodeConfig conf2(std::string("./core_tests/conf/conf_taraxa2.json"));
    conf2.network.network_id = "main";
    auto node2 = taraxa::FullNode::make(conf2, true);

    node2->setDebug(true);
    node2->start(false);  // boot node

    taraxa::thisThreadSleepForMilliSeconds(1000);
    EXPECT_EQ(node1->getPeerCount(), 1);
    EXPECT_EQ(node2->getPeerCount(), 1);
  }
  {
    FullNodeConfig conf1(std::string("./core_tests/conf/conf_taraxa1.json"));
    conf1.network.network_id = "main";
    auto node1(taraxa::FullNode::make(conf1, true));

    node1->setDebug(true);
    node1->start(true);

    FullNodeConfig conf2(std::string("./core_tests/conf/conf_taraxa2.json"));
    conf2.network.network_id = "testnet";
    auto node2 = taraxa::FullNode::make(conf2, true);

    node2->setDebug(true);
    node2->start(false);  // boot node

    taraxa::thisThreadSleepForMilliSeconds(1000);
    EXPECT_EQ(node1->getPeerCount(), 0);
    EXPECT_EQ(node2->getPeerCount(), 0);
  }
}

/*
Test creates a DAG on one node and verifies
that the second node syncs with it and that the resulting
DAG on the other end is the same
*/
TEST_F(NetworkTest, node_sync) {
  auto node1(taraxa::FullNode::make(
      std::string("./core_tests/conf/conf_taraxa1.json"), true));

  node1->setDebug(true);
  node1->start(true);

  // Allow node to start up
  taraxa::thisThreadSleepForMilliSeconds(1000);

  std::vector<DagBlock> blks;

  DagBlock blk1(node1->getConfig().chain.dag_genesis_block.getHash(), 1, {}, {},
                sig_t(777), blk_hash_t(1), addr_t(999));

  DagBlock blk2(blk_hash_t(01), 2, {}, {}, sig_t(777), blk_hash_t(02),
                addr_t(999));

  DagBlock blk3(blk_hash_t(02), 3, {}, {}, sig_t(777), blk_hash_t(03),
                addr_t(999));

  DagBlock blk4(blk_hash_t(03), 4, {}, {}, sig_t(777), blk_hash_t(04),
                addr_t(999));

  DagBlock blk5(blk_hash_t(04), 5, {}, {}, sig_t(777), blk_hash_t(05),
                addr_t(999));

  DagBlock blk6(blk_hash_t(05), 6, {blk_hash_t(04), blk_hash_t(3)}, {},
                sig_t(777), blk_hash_t(06), addr_t(999));

  blks.push_back(blk6);
  blks.push_back(blk5);
  blks.push_back(blk4);
  blks.push_back(blk3);
  blks.push_back(blk2);
  blks.push_back(blk1);

  for (auto i = 0; i < blks.size(); ++i) {
    node1->insertBlock(blks[i]);
  }

  taraxa::thisThreadSleepForMilliSeconds(1000);

  auto node2 = taraxa::FullNode::make(
      std::string("./core_tests/conf/conf_taraxa2.json"), true);

  node2->setDebug(true);
  node2->start(false);  // boot node

  std::cout << "Waiting Sync for max 20000 milliseconds ..." << std::endl;
  for (int i = 0; i < 20; i++) {
    taraxa::thisThreadSleepForMilliSeconds(1000);
    if (node2->getNumVerticesInDag().first == 7 &&
        node2->getNumEdgesInDag().first == 8)
      break;
  }

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
TEST_F(NetworkTest, node_pbft_sync) {
  auto node1(taraxa::FullNode::make(
      std::string("./core_tests/conf/conf_taraxa1.json"), true));
  node1->start(true);  // boot node

  // generate first PBFT block sample
  blk_hash_t prev_block_hash(0);
  blk_hash_t dag_blk(123);

  DagBlock blk1(node1->getConfig().chain.dag_genesis_block.getHash(), 1, {}, {},
                sig_t(777), blk_hash_t(12), addr_t(999));
  DagBlock blk2(blk_hash_t(12), 2, {}, {}, sig_t(777), blk_hash_t(123),
                addr_t(999));

  DagBlock blk3(blk_hash_t(123), 2, {}, {}, sig_t(777), blk_hash_t(456),
                addr_t(999));

  node1->getDB()->saveDagBlock(blk1);
  node1->getDB()->saveDagBlock(blk2);
  node1->getDB()->saveDagBlock(blk3);

  TrxSchedule schedule;
  uint64_t period = 1;
  uint64_t height = 2;
  uint64_t timestamp = std::time(nullptr);
  addr_t beneficiary(987);
  PbftBlock pbft_block1(prev_block_hash, dag_blk, schedule, period, height,
                        timestamp, beneficiary);
  sig_t signature = node1->signMessage(pbft_block1.getJsonStr(false));
  pbft_block1.setSignature(signature);
  pbft_block1.setBlockHash();

  std::vector<Vote> votes_for_pbft_blk1;
  votes_for_pbft_blk1.emplace_back(node1->generateVote(
      pbft_block1.getBlockHash(), cert_vote_type, 1, 3, prev_block_hash));
  std::cout << "Generate 1 vote for first PBFT block" << std::endl;

  node1->storeCertVotes(pbft_block1.getBlockHash(), votes_for_pbft_blk1);
  std::shared_ptr<PbftChain> pbft_chain1 = node1->getPbftChain();
  // node1 put block1 into pbft chain and store into DB
  bool push_block = pbft_chain1->pushPbftBlock(pbft_block1);
  EXPECT_TRUE(push_block);
  EXPECT_EQ(node1->getPbftChainSize(), 2);

  // generate second PBFT block sample
  prev_block_hash = pbft_block1.getBlockHash();
  dag_blk = blk_hash_t(456);
  period = 2;
  height = 3;
  timestamp = std::time(nullptr);
  beneficiary = addr_t(654);
  PbftBlock pbft_block2(prev_block_hash, dag_blk, schedule, period, height,
                        timestamp, beneficiary);
  signature = node1->signMessage(pbft_block2.getJsonStr(false));
  pbft_block2.setSignature(signature);
  pbft_block2.setBlockHash();

  std::vector<Vote> votes_for_pbft_blk2;
  votes_for_pbft_blk2.emplace_back(node1->generateVote(
      pbft_block2.getBlockHash(), cert_vote_type, 2, 3, prev_block_hash));
  std::cout << "Generate 1 vote for second PBFT block" << std::endl;

  node1->storeCertVotes(pbft_block2.getBlockHash(), votes_for_pbft_blk2);

  // node1 put block2 into pbft chain and store into DB
  push_block = pbft_chain1->pushPbftBlock(pbft_block2);
  EXPECT_TRUE(push_block);
  EXPECT_EQ(node1->getPbftChainSize(), 3);

  auto node2 = taraxa::FullNode::make(
      std::string("./core_tests/conf/conf_taraxa2.json"), true);
  node2->start(false);  // boot node

  std::shared_ptr<Network> nw1 = node1->getNetwork();
  std::shared_ptr<Network> nw2 = node2->getNetwork();
  const int node_peers = 1;
  bool checkpoint_passed = false;
  const int timeout_val = 60;
  for (auto i = 0; i < timeout_val; i++) {
    // test timeout is 60 seconds
    if (nw1->getPeerCount() == node_peers &&
        nw2->getPeerCount() == node_peers) {
      checkpoint_passed = true;
      break;
    }
    taraxa::thisThreadSleepForMilliSeconds(1000);
  }
  if (checkpoint_passed == false) {
    std::cout << "Timeout reached after " << timeout_val << " seconds..."
              << std::endl;
    ASSERT_EQ(node_peers, nw1->getPeerCount());
    ASSERT_EQ(node_peers, nw2->getPeerCount());
  }

  std::cout << "Waiting Sync for max 2 minutes..." << std::endl;
  for (int i = 0; i < 1200; i++) {
    if (node2->getPbftChainSize() == 3) {
      break;
    }
    taraxa::thisThreadSleepForMilliSeconds(100);
  }
  EXPECT_EQ(node2->getPbftChainSize(), 3);
  std::shared_ptr<PbftChain> pbft_chain2 = node2->getPbftChain();
  blk_hash_t second_pbft_block_hash = pbft_chain2->getLastPbftBlockHash();
  EXPECT_EQ(second_pbft_block_hash, pbft_block2.getBlockHash());
  blk_hash_t first_pbft_block_hash =
      pbft_chain2->getPbftBlockInChain(second_pbft_block_hash)
          .getPrevBlockHash();
  EXPECT_EQ(first_pbft_block_hash, pbft_block1.getBlockHash());
}

TEST_F(NetworkTest, node_pbft_sync_without_enough_votes) {
  auto node1(taraxa::FullNode::make(
      std::string("./core_tests/conf/conf_taraxa1.json"), true));
  node1->start(true);  // boot node

  // generate first PBFT block sample
  blk_hash_t prev_block_hash(0);
  blk_hash_t dag_blk(234);
  TrxSchedule schedule;
  uint64_t period = 1;
  uint64_t height = 2;
  uint64_t timestamp = std::time(nullptr);
  addr_t beneficiary(876);
  PbftBlock pbft_block1(prev_block_hash, dag_blk, schedule, period, height,
                        timestamp, beneficiary);
  sig_t signature = node1->signMessage(pbft_block1.getJsonStr(false));
  pbft_block1.setSignature(signature);
  pbft_block1.setBlockHash();
  std::vector<Vote> votes_for_pbft_blk1;
  votes_for_pbft_blk1.emplace_back(node1->generateVote(
      pbft_block1.getBlockHash(), cert_vote_type, 1, 3, prev_block_hash));
  std::cout << "Generate 1 vote for first PBFT block" << std::endl;

  node1->storeCertVotes(pbft_block1.getBlockHash(), votes_for_pbft_blk1);
  std::shared_ptr<PbftChain> pbft_chain1 = node1->getPbftChain();
  // node1 put block1 into pbft chain and store into DB
  bool push_block = pbft_chain1->pushPbftBlock(pbft_block1);
  EXPECT_TRUE(push_block);
  EXPECT_EQ(node1->getPbftChainSize(), 2);

  // generate second PBFT block sample
  prev_block_hash = pbft_block1.getBlockHash();
  dag_blk = blk_hash_t(567);
  period = 2;
  height = 3;
  timestamp = std::time(nullptr);
  beneficiary = addr_t(543);
  PbftBlock pbft_block2(prev_block_hash, dag_blk, schedule, period, height,
                        timestamp, beneficiary);
  signature = node1->signMessage(pbft_block2.getJsonStr(false));
  pbft_block2.setSignature(signature);
  pbft_block2.setBlockHash();

  std::cout << "There are no votes for the second PBFT block" << std::endl;

  // node1 put block2 into pbft chain and no votes store into DB
  // (malicious player)
  push_block = pbft_chain1->pushPbftBlock(pbft_block2);
  EXPECT_TRUE(push_block);
  EXPECT_EQ(node1->getPbftChainSize(), 3);

  auto node2 = taraxa::FullNode::make(
      std::string("./core_tests/conf/conf_taraxa4.json"), true);
  node2->start(false);  // boot node

  std::shared_ptr<Network> nw1 = node1->getNetwork();
  std::shared_ptr<Network> nw2 = node2->getNetwork();
  const int node_peers = 1;
  bool checkpoint_passed = false;
  const int timeout_val = 60;
  for (auto i = 0; i < timeout_val; i++) {
    // test timeout is 60 seconds
    if (nw1->getPeerCount() == node_peers &&
        nw2->getPeerCount() == node_peers) {
      checkpoint_passed = true;
      break;
    }
    taraxa::thisThreadSleepForMilliSeconds(1000);
  }
  if (checkpoint_passed == false) {
    std::cout << "Timeout reached after " << timeout_val << " seconds..."
              << std::endl;
    ASSERT_EQ(node_peers, nw1->getPeerCount());
    ASSERT_EQ(node_peers, nw2->getPeerCount());
  }

  std::cout << "Waiting Sync for max 1 minutes..." << std::endl;
  for (int i = 0; i < 600; i++) {
    if (node2->getPbftChainSize() >= 2) {
      break;
    }
    taraxa::thisThreadSleepForMilliSeconds(100);
  }
  EXPECT_EQ(node2->getPbftChainSize(), 2);
  std::shared_ptr<PbftChain> pbft_chain2 = node2->getPbftChain();
  blk_hash_t last_pbft_block_hash = pbft_chain2->getLastPbftBlockHash();
  EXPECT_EQ(last_pbft_block_hash, pbft_block1.getBlockHash());
}

/*
Test creates a DAG on one node and verifies
that the second node syncs with it and that the resulting
DAG on the other end is the same
Unlike the previous tests, this DAG contains blocks with transactions
and verifies that the sync containing transactions is successful
*/
TEST_F(NetworkTest, node_sync_with_transactions) {
  auto node1(taraxa::FullNode::make(
      std::string("./core_tests/conf/conf_taraxa1.json"), true));

  node1->setDebug(true);
  node1->start(true);
  std::vector<DagBlock> blks;

  DagBlock blk1(
      node1->getConfig().chain.dag_genesis_block.getHash(), 1, {},
      {g_signed_trx_samples[0].getHash(), g_signed_trx_samples[1].getHash()},
      sig_t(777), blk_hash_t(1), addr_t(999));
  std::vector<Transaction> tr1(
      {g_signed_trx_samples[0], g_signed_trx_samples[1]});

  DagBlock blk2(blk_hash_t(01), 2, {}, {g_signed_trx_samples[2].getHash()},
                sig_t(777), blk_hash_t(02), addr_t(999));
  std::vector<Transaction> tr2({g_signed_trx_samples[2]});

  DagBlock blk3(blk_hash_t(02), 3, {}, {}, sig_t(777), blk_hash_t(03),
                addr_t(999));
  std::vector<Transaction> tr3;

  DagBlock blk4(
      blk_hash_t(03), 4, {},
      {g_signed_trx_samples[3].getHash(), g_signed_trx_samples[4].getHash()},
      sig_t(777), blk_hash_t(04), addr_t(999));
  std::vector<Transaction> tr4(
      {g_signed_trx_samples[3], g_signed_trx_samples[4]});

  DagBlock blk5(
      blk_hash_t(04), 5, {},
      {g_signed_trx_samples[5].getHash(), g_signed_trx_samples[6].getHash(),
       g_signed_trx_samples[7].getHash(), g_signed_trx_samples[8].getHash()},
      sig_t(777), blk_hash_t(05), addr_t(999));
  std::vector<Transaction> tr5(
      {g_signed_trx_samples[5], g_signed_trx_samples[6],
       g_signed_trx_samples[7], g_signed_trx_samples[8]});

  DagBlock blk6(blk_hash_t(05), 6, {blk_hash_t(04), blk_hash_t(3)}, {},
                sig_t(777), blk_hash_t(06), addr_t(999));
  std::vector<Transaction> tr6;

  node1->insertBroadcastedBlockWithTransactions(blk6, tr6);
  node1->insertBroadcastedBlockWithTransactions(blk5, tr5);
  node1->insertBroadcastedBlockWithTransactions(blk4, tr4);
  node1->insertBroadcastedBlockWithTransactions(blk3, tr3);
  node1->insertBroadcastedBlockWithTransactions(blk2, tr2);
  node1->insertBroadcastedBlockWithTransactions(blk1, tr1);

  // To make sure blocks are stored before starting node 2
  taraxa::thisThreadSleepForMilliSeconds(1000);

  auto node2 = taraxa::FullNode::make(
      std::string("./core_tests/conf/conf_taraxa2.json"), true);

  node2->setDebug(true);
  node2->start(false);  // boot node

  std::cout << "Waiting Sync for up to 20000 milliseconds ..." << std::endl;
  for (int i = 0; i < 20; i++) {
    taraxa::thisThreadSleepForMilliSeconds(1000);
    if (node2->getNumVerticesInDag().first == 7 &&
        node2->getNumEdgesInDag().first == 8)
      break;
  }

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
TEST_F(NetworkTest, node_sync2) {
  taraxa::thisThreadSleepForMilliSeconds(2000);

  auto node1(taraxa::FullNode::make(
      std::string("./core_tests/conf/conf_taraxa1.json"), true));

  node1->setDebug(true);
  node1->start(true);
  std::vector<DagBlock> blks;

  auto transactions = samples::createSignedTrxSamples(0, NUM_TRX2, g_secret2);
  DagBlock blk1(node1->getConfig().chain.dag_genesis_block.getHash(), 1, {},
                {transactions[0].getHash(), transactions[1].getHash()},
                sig_t(777), blk_hash_t(0xB1), addr_t(999));
  std::vector<Transaction> tr1({transactions[0], transactions[1]});

  DagBlock blk2(node1->getConfig().chain.dag_genesis_block.getHash(), 1, {},
                {transactions[2].getHash(), transactions[3].getHash()},
                sig_t(777), blk_hash_t(0xB2), addr_t(999));
  std::vector<Transaction> tr2({transactions[2], transactions[3]});

  DagBlock blk3(blk_hash_t(0xB1), 2, {},
                {transactions[4].getHash(), transactions[5].getHash()},
                sig_t(777), blk_hash_t(0xB6), addr_t(999));
  std::vector<Transaction> tr3({transactions[4], transactions[5]});

  DagBlock blk4(blk_hash_t(0xB6), 7, {},
                {transactions[6].getHash(), transactions[7].getHash()},
                sig_t(777), blk_hash_t(0xB10), addr_t(999));
  std::vector<Transaction> tr4({transactions[6], transactions[7]});

  DagBlock blk5(blk_hash_t(0xB2), 3, {},
                {transactions[8].getHash(), transactions[9].getHash()},
                sig_t(777), blk_hash_t(0xB8), addr_t(999));
  std::vector<Transaction> tr5({transactions[8], transactions[9]});

  DagBlock blk6(blk_hash_t(0xB1), 2, {},
                {transactions[10].getHash(), transactions[11].getHash()},
                sig_t(777), blk_hash_t(0xB3), addr_t(999));
  std::vector<Transaction> tr6({transactions[10], transactions[11]});

  DagBlock blk7(blk_hash_t(0xB3), 4, {},
                {transactions[12].getHash(), transactions[13].getHash()},
                sig_t(777), blk_hash_t(0xB4), addr_t(999));
  std::vector<Transaction> tr7({transactions[12], transactions[13]});

  DagBlock blk8(blk_hash_t(0xB1), 5, {blk_hash_t(0xB4)},
                {transactions[14].getHash(), transactions[15].getHash()},
                sig_t(777), blk_hash_t(0xB5), addr_t(999));
  std::vector<Transaction> tr8({transactions[14], transactions[15]});

  DagBlock blk9(blk_hash_t(0xB1), 2, {},
                {transactions[16].getHash(), transactions[17].getHash()},
                sig_t(777), blk_hash_t(0xB7), addr_t(999));
  std::vector<Transaction> tr9({transactions[16], transactions[17]});

  DagBlock blk10(blk_hash_t(0xB5), 6, {},
                 {transactions[18].getHash(), transactions[19].getHash()},
                 sig_t(777), blk_hash_t(0xB9), addr_t(999));
  std::vector<Transaction> tr10({transactions[18], transactions[19]});

  DagBlock blk11(blk_hash_t(0xB6), 7, {},
                 {transactions[20].getHash(), transactions[21].getHash()},
                 sig_t(777), blk_hash_t(0xB11), addr_t(999));
  std::vector<Transaction> tr11({transactions[20], transactions[21]});

  DagBlock blk12(blk_hash_t(0xB8), 8, {},
                 {transactions[22].getHash(), transactions[23].getHash()},
                 sig_t(777), blk_hash_t(0xB12), addr_t(999));
  std::vector<Transaction> tr12({transactions[22], transactions[23]});

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
    node1->insertBroadcastedBlockWithTransactions(blks[i], trxs[i]);
  }

  taraxa::thisThreadSleepForMilliSeconds(2000);

  auto node2(taraxa::FullNode::make(
      std::string("./core_tests/conf/conf_taraxa2.json"), true));

  node2->setDebug(true);
  node2->start(false);  // boot node

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
}

/*
Test creates new transactions on one node and verifies
that the second node receives the transactions
*/
TEST_F(NetworkTest, node_transaction_sync) {
  auto node1(taraxa::FullNode::make(
      std::string("./core_tests/conf/conf_taraxa1.json"), true));

  node1->setDebug(true);
  node1->start(true);

  std::vector<taraxa::bytes> transactions;
  for (auto const& t : *g_signed_trx_samples) {
    transactions.emplace_back(t.rlp(true));
  }

  node1->insertBroadcastedTransactions(transactions);

  taraxa::thisThreadSleepForMilliSeconds(1000);

  auto node2 = taraxa::FullNode::make(
      std::string("./core_tests/conf/conf_taraxa2.json"), true);

  node2->setDebug(true);
  node2->start(false);  // boot node

  std::cout << "Waiting Sync for 2000 milliseconds ..." << std::endl;
  taraxa::thisThreadSleepForMilliSeconds(2000);

  for (auto const& t : *g_signed_trx_samples) {
    EXPECT_TRUE(node2->getTransaction(t.getHash()) != nullptr);
    if (node2->getTransaction(t.getHash()) != nullptr) {
      EXPECT_EQ(t, node2->getTransaction(t.getHash())->first);
    }
  }
}

/*
Test creates multiple nodes and creates new transactions in random time
intervals on randomly selected nodes It verifies that the blocks created from
these transactions which get created on random nodes are synced and the
resulting DAG is the same on all nodes
*/
// fixme: flaky
TEST_F(NetworkTest, node_full_sync) {
  const int numberOfNodes = 5;
  auto node1(taraxa::FullNode::make(
      std::string("./core_tests/conf/conf_taraxa1.json"), true));

  node1->setDebug(true);
  node1->start(true);  // boot node

  std::vector<FullNode::Handle> nodes;
  for (int i = 0; i < numberOfNodes; i++) {
    FullNodeConfig config(std::string("./core_tests/conf/conf_taraxa2.json"));
    config.db_path += std::to_string(i + 1);
    config.network.network_listen_port += i + 1;
    config.node_secret = "";
    nodes.push_back(taraxa::FullNode::make(config, true));
    nodes[i]->start(false);  // boot node
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
  std::vector<Transaction> ts;
  for (auto i = 0; i < NUM_TRX2; ++i) {
    ts.push_back(samples::TX_GEN->getWithRandomUniqueSender());
  }
  for (auto i = 0; i < NUM_TRX2/2; ++i) {
    std::vector<taraxa::bytes> transactions;
    transactions.emplace_back(ts[i].rlp(true));
    nodes[distNodes(rng)]->insertBroadcastedTransactions(transactions);
    thisThreadSleepForMilliSeconds(distTransactions(rng));
    counter++;
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

  FullNodeConfig config(std::string("./core_tests/conf/conf_taraxa2.json"));
  config.db_path += std::to_string(numberOfNodes + 1);
  config.network.network_listen_port += numberOfNodes + 1;
  config.node_secret = "";
  nodes.push_back(taraxa::FullNode::make(config, true));
  nodes[numberOfNodes]->start(false);  // boot node
  taraxa::thisThreadSleepForMilliSeconds(50);

  for (auto i = NUM_TRX2/2; i < NUM_TRX2; ++i) {
    std::vector<taraxa::bytes> transactions;
    transactions.emplace_back(ts[i].rlp(true));
    nodes[distNodes(rng)]->insertBroadcastedTransactions(transactions);
    thisThreadSleepForMilliSeconds(distTransactions(rng));
    counter++;
  }

  std::cout << "Waiting Sync for up to 2 minutes ..." << std::endl;
  for (int i = 0; i < 24; i++) {
    taraxa::thisThreadSleepForMilliSeconds(5000);
    bool finished = true;
    for (int j = 0; j < numberOfNodes + 1; j++) {
      if (nodes[j]->getNumVerticesInDag().first !=
          node1->getNumVerticesInDag().first) {
        finished = false;
        break;
      }
      if (!nodes[j]->isSynced()) {
        finished = false;
      }
    }
    if (finished)
      break;
    else
      printf("Waiting %d\n", i);
  }

  EXPECT_GT(node1->getNumVerticesInDag().first, 0);
  EXPECT_GT(10, node1->getVerifiedTrxSnapShot().size());
  for (int i = 0; i < numberOfNodes + 1; i++) {
    EXPECT_GT(nodes[i]->getNumVerticesInDag().first, 0);
    EXPECT_EQ(nodes[i]->getNumVerticesInDag().first,
              node1->getNumVerticesInDag().first);
    EXPECT_EQ(nodes[i]->getNumVerticesInDag().first,
              nodes[i]->getNumDagBlocks());
    EXPECT_EQ(nodes[i]->getNumEdgesInDag().first,
              node1->getNumEdgesInDag().first);
    EXPECT_TRUE(nodes[i]->isSynced());
  }

  // Write any DAG diff
  for (int i = 0; i < numberOfNodes + 1; i++) {
    uint64_t level = 1;
    while (true) {
      auto blocks1 = node1->getDagBlocksAtLevel(level, 1);
      auto blocks2 = nodes[i]->getDagBlocksAtLevel(level, 1);
      if (blocks1.size() != blocks2.size()) {
        printf("DIFF at level %lu: \n", level);
        for (auto b : blocks1) printf(" %s", b->getHash().toString().c_str());
        printf("\n");
        for (auto b : blocks2) printf(" %s", b->getHash().toString().c_str());
        printf("\n");
      }
      if (blocks1.size() == 0 && blocks2.size() == 0) break;
      level++;
    }
  }
}

}  // namespace taraxa

int main(int argc, char** argv) {
  taraxa::static_init();
  dev::LoggingOptions logOptions;
  logOptions.verbosity = dev::VerbosityError;
  // logOptions.includeChannels.push_back("PBFTSYNC");
  // logOptions.includeChannels.push_back("DAGSYNC");
  // logOptions.includeChannels.push_back("NETWORK");
  // logOptions.includeChannels.push_back("TARCAP");
  dev::setupLogging(logOptions);
  dev::db::setDatabaseKind(dev::db::DatabaseKind::RocksDB);
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}