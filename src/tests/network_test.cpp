#include "network.hpp"

#include <gtest/gtest.h>

#include <atomic>
#include <iostream>
#include <vector>

#include "../block_proposer.hpp"
#include "../dag.hpp"
#include "../pbft_manager.hpp"
#include "../static_init.hpp"
#include "../util/lazy.hpp"
#include "../util_test/samples.hpp"
#include "../util_test/util.hpp"

namespace taraxa::core_tests {

const unsigned NUM_TRX = 10;
auto g_secret = Lazy(
    [] { return dev::Secret("3800b2875669d9b2053c1aff9224ecfdc411423aac5b5a73d7a45ced1c3b9dcd", dev::Secret::ConstructFromStringType::FromHex); });
auto node_key = dev::KeyPair(g_secret);

auto g_trx_samples = Lazy([] { return samples::createMockTrxSamples(0, NUM_TRX); });
auto g_signed_trx_samples = Lazy([] { return samples::createSignedTrxSamples(0, NUM_TRX, g_secret); });

const unsigned NUM_TRX2 = 200;
auto g_secret2 = Lazy(
    [] { return dev::Secret("3800b2875669d9b2053c1aff9224ecfdc411423aac5b5a73d7a45ced1c3b9dcd", dev::Secret::ConstructFromStringType::FromHex); });
auto g_trx_samples2 = Lazy([] { return samples::createMockTrxSamples(0, NUM_TRX2); });
auto g_signed_trx_samples2 = Lazy([] { return samples::createSignedTrxSamples(0, NUM_TRX2, g_secret2); });
auto three_default_configs = Lazy([] { return make_node_cfgs(3); });
auto g_conf1 = Lazy([] { return three_default_configs[0]; });
auto g_conf2 = Lazy([] { return three_default_configs[1]; });
auto g_conf3 = Lazy([] { return three_default_configs[2]; });

struct NetworkTest : BaseTest {};

// Test creates two Network setup and verifies sending block
// between is successfull
TEST_F(NetworkTest, transfer_block) {
  std::shared_ptr<Network> nw1(new taraxa::Network(g_conf1->network, g_conf1->chain.dag_genesis_block.getHash().toString(), addr_t()));
  std::shared_ptr<Network> nw2(new taraxa::Network(g_conf2->network, g_conf2->chain.dag_genesis_block.getHash().toString(), addr_t()));

  nw1->start();
  nw2->start();
  DagBlock blk(blk_hash_t(1111), 0, {blk_hash_t(222), blk_hash_t(333), blk_hash_t(444)},
               {g_signed_trx_samples[0].getHash(), g_signed_trx_samples[1].getHash()}, sig_t(7777), blk_hash_t(888), addr_t(999));

  std::vector<taraxa::bytes> transactions;
  transactions.emplace_back(*g_signed_trx_samples[0].rlp());
  transactions.emplace_back(*g_signed_trx_samples[1].rlp());
  nw2->onNewTransactions(transactions);

  taraxa::thisThreadSleepForSeconds(1);

  for (auto i = 0; i < 1; ++i) {
    nw2->sendBlock(nw1->getNodeId(), blk, true);
  }

  std::cout << "Waiting packages for 10 seconds ..." << std::endl;

  for (int i = 0; i < 100; i++) {
    if (nw1->getReceivedBlocksCount()) break;
    taraxa::thisThreadSleepForMilliSeconds(100);
  }
  nw2->stop();
  unsigned long long num_received = nw1->getReceivedBlocksCount();
  nw1->stop();
  ASSERT_EQ(1, num_received);
}

TEST_F(NetworkTest, send_pbft_block) {
  std::shared_ptr<Network> nw1(new taraxa::Network(g_conf1->network, g_conf1->chain.dag_genesis_block.getHash().toString(), addr_t()));
  std::shared_ptr<Network> nw2(new taraxa::Network(g_conf2->network, g_conf2->chain.dag_genesis_block.getHash().toString(), addr_t()));

  nw1->start();
  nw2->start();
  auto pbft_block = make_simple_pbft_block(blk_hash_t(1), 2);
  uint64_t chain_size = 111;
  taraxa::thisThreadSleepForSeconds(1);

  nw2->sendPbftBlock(nw1->getNodeId(), pbft_block, chain_size);
  taraxa::thisThreadSleepForMilliSeconds(200);

  ASSERT_EQ(1, nw1->getTaraxaCapability()->getAllPeers().size());
  ASSERT_EQ(chain_size, nw1->getTaraxaCapability()->getPeer(nw1->getTaraxaCapability()->getAllPeers()[0])->pbft_chain_size_);
  nw2->stop();
  nw1->stop();
}

// Test creates two Network setup and verifies sending transaction
// between is successfull
TEST_F(NetworkTest, transfer_transaction) {
  std::shared_ptr<Network> nw1(new taraxa::Network(g_conf1->network, g_conf1->chain.dag_genesis_block.getHash().toString(), addr_t()));
  std::shared_ptr<Network> nw2(new taraxa::Network(g_conf2->network, g_conf2->chain.dag_genesis_block.getHash().toString(), addr_t()));

  nw1->start(true);
  nw2->start();
  std::vector<taraxa::bytes> transactions;
  transactions.push_back(*g_signed_trx_samples[0].rlp());
  transactions.push_back(*g_signed_trx_samples[1].rlp());
  transactions.push_back(*g_signed_trx_samples[2].rlp());

  taraxa::thisThreadSleepForSeconds(1);

  nw2->sendTransactions(nw1->getNodeId(), transactions);

  std::cout << "Waiting packages for 10 seconds ..." << std::endl;

  for (int i = 0; i < 10; i++) {
    if (nw1->getReceivedTransactionsCount()) break;
    taraxa::thisThreadSleepForSeconds(1);
  }

  nw2->stop();
  unsigned long long num_received = nw1->getReceivedTransactionsCount();
  nw1->stop();
  ASSERT_EQ(3, num_received);
}

// Test verifies saving network to a file and restoring it from a file
// is successfull. Once restored from the file it is able to reestablish
// connections even with boot nodes down
TEST_F(NetworkTest, save_network) {
  {
    std::shared_ptr<Network> nw1(new taraxa::Network(g_conf1->network, g_conf1->chain.dag_genesis_block.getHash().toString(), addr_t()));
    std::shared_ptr<Network> nw2(new taraxa::Network(g_conf2->network, g_conf2->chain.dag_genesis_block.getHash().toString(), addr_t()));
    std::shared_ptr<Network> nw3(new taraxa::Network(g_conf3->network, g_conf3->chain.dag_genesis_block.getHash().toString(), addr_t()));

    nw1->start(true);
    nw2->start();
    nw3->start();

    for (int i = 0; i < 45; i++) {
      taraxa::thisThreadSleepForSeconds(1);
      if (2 == nw1->getPeerCount() && 2 == nw2->getPeerCount() && 2 == nw3->getPeerCount()) break;
    }

    ASSERT_EQ(2, nw1->getPeerCount());
    ASSERT_EQ(2, nw2->getPeerCount());
    ASSERT_EQ(2, nw3->getPeerCount());

    nw1->stop();
    nw2->stop();
    nw3->stop();
    nw2->saveNetwork("/tmp/nw2");
    nw3->saveNetwork("/tmp/nw3");
  }

  std::shared_ptr<Network> nw2(new taraxa::Network(g_conf2->network, "/tmp/nw2", g_conf2->chain.dag_genesis_block.getHash().toString(), addr_t(),
                                                   nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, public_t(), 2000));
  std::shared_ptr<Network> nw3(new taraxa::Network(g_conf3->network, "/tmp/nw3", g_conf2->chain.dag_genesis_block.getHash().toString(), addr_t(),
                                                   nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, public_t(), 2000));
  nw2->start();
  nw3->start();

  for (int i = 0; i < 20; i++) {
    taraxa::thisThreadSleepForSeconds(1);
    if (1 == nw2->getPeerCount() && 1 == nw3->getPeerCount()) break;
  }

  ASSERT_EQ(1, nw2->getPeerCount());
  ASSERT_EQ(1, nw3->getPeerCount());
}

// Test creates one node with testnet network ID and one node with main ID and
// verifies that connection fails
TEST_F(NetworkTest, node_network_id) {
  auto node_cfgs = make_node_cfgs(2);
  {
    auto node_cfgs_ = node_cfgs;
    node_cfgs_[0].network.network_id = 1;
    node_cfgs_[1].network.network_id = 1;
    auto nodes = launch_nodes(node_cfgs_);
  }
  {
    auto conf1 = node_cfgs[0];
    conf1.network.network_id = 1;
    FullNode::Handle node1(conf1, true);

    auto conf2 = node_cfgs[1];
    conf2.network.network_id = 2;
    FullNode::Handle node2(conf2, true);

    taraxa::thisThreadSleepForMilliSeconds(1000);
    EXPECT_EQ(node1->getNetwork()->getPeerCount(), 0);
    EXPECT_EQ(node2->getNetwork()->getPeerCount(), 0);
  }
}

// Test creates a DAG on one node and verifies
// that the second node syncs with it and that the resulting
// DAG on the other end is the same
TEST_F(NetworkTest, node_sync) {
  auto node_cfgs = make_node_cfgs<5>(2);
  FullNode::Handle node1(node_cfgs[0], true);

  // Allow node to start up
  taraxa::thisThreadSleepForMilliSeconds(1000);

  std::vector<DagBlock> blks;
  // Generate DAG blocks
  auto dag_genesis = node1->getConfig().chain.dag_genesis_block.getHash();
  auto sk = node1->getSecretKey();
  auto vrf_sk = node1->getVrfSecretKey();
  vdf_sortition::VdfConfig vdf_config(255, 0, 15, 10, 1500);

  auto propose_level = 1;
  vdf_sortition::Message msg1(propose_level);
  vdf_sortition::VdfSortition vdf1(vdf_config, node_key.address(), vrf_sk, msg1);
  vdf1.computeVdfSolution(dag_genesis.toString());
  DagBlock blk1(dag_genesis, propose_level, {}, {}, vdf1);
  blk1.sign(sk);

  propose_level = 2;
  vdf_sortition::Message msg2(propose_level);
  vdf_sortition::VdfSortition vdf2(vdf_config, node_key.address(), vrf_sk, msg2);
  vdf2.computeVdfSolution(blk1.getHash().toString());
  DagBlock blk2(blk1.getHash(), propose_level, {}, {}, vdf2);
  blk2.sign(sk);

  propose_level = 3;
  vdf_sortition::Message msg3(propose_level);
  vdf_sortition::VdfSortition vdf3(vdf_config, node_key.address(), vrf_sk, msg3);
  vdf3.computeVdfSolution(blk2.getHash().toString());
  DagBlock blk3(blk2.getHash(), propose_level, {}, {}, vdf3);
  blk3.sign(sk);

  propose_level = 4;
  vdf_sortition::Message msg4(propose_level);
  vdf_sortition::VdfSortition vdf4(vdf_config, node_key.address(), vrf_sk, msg4);
  vdf4.computeVdfSolution(blk3.getHash().toString());
  DagBlock blk4(blk3.getHash(), propose_level, {}, {}, vdf4);
  blk4.sign(sk);

  propose_level = 5;
  vdf_sortition::Message msg5(propose_level);
  vdf_sortition::VdfSortition vdf5(vdf_config, node_key.address(), vrf_sk, msg5);
  vdf5.computeVdfSolution(blk4.getHash().toString());
  DagBlock blk5(blk4.getHash(), propose_level, {}, {}, vdf5);
  blk5.sign(sk);

  propose_level = 6;
  vdf_sortition::Message msg6(propose_level);
  vdf_sortition::VdfSortition vdf6(vdf_config, node_key.address(), vrf_sk, msg6);
  vdf6.computeVdfSolution(blk5.getHash().toString());
  DagBlock blk6(blk5.getHash(), propose_level, {blk4.getHash(), blk3.getHash()}, {}, vdf6);
  blk6.sign(sk);

  blks.push_back(blk6);
  blks.push_back(blk5);
  blks.push_back(blk4);
  blks.push_back(blk3);
  blks.push_back(blk2);
  blks.push_back(blk1);

  for (auto i = 0; i < blks.size(); ++i) {
    node1->getBlockManager()->insertBlock(blks[i]);
  }

  EXPECT_HAPPENS({30s, 500ms}, [&](auto& ctx) {
    WAIT_EXPECT_EQ(ctx, node1->getNumReceivedBlocks(), blks.size());
    WAIT_EXPECT_EQ(ctx, node1->getDagManager()->getNumVerticesInDag().first, 7);
    WAIT_EXPECT_EQ(ctx, node1->getDagManager()->getNumEdgesInDag().first, 8);
  });

  FullNode::Handle node2(node_cfgs[1], true);

  std::cout << "Waiting Sync..." << std::endl;
  EXPECT_HAPPENS({45s, 1500ms}, [&](auto& ctx) {
    WAIT_EXPECT_EQ(ctx, node2->getNumReceivedBlocks(), blks.size());
    WAIT_EXPECT_EQ(ctx, node2->getDagManager()->getNumVerticesInDag().first, 7);
    WAIT_EXPECT_EQ(ctx, node2->getDagManager()->getNumEdgesInDag().first, 8);
  });
}

// Test creates a PBFT chain on one node and verifies
// that the second node syncs with it and that the resulting
// chain on the other end is the same
TEST_F(NetworkTest, node_pbft_sync) {
  auto node_cfgs = make_node_cfgs(2);
  FullNode::Handle node1(node_cfgs[0], true);

  node1->getPbftManager()->stop();
  auto db1 = node1->getDB();
  auto pbft_chain1 = node1->getPbftChain();

  auto dag_genesis = node1->getConfig().chain.dag_genesis_block.getHash();
  auto sk = node1->getSecretKey();
  auto vrf_sk = node1->getVrfSecretKey();
  vdf_sortition::VdfConfig vdf_config(255, 0, 15, 10, 1500);
  auto batch = db1->createWriteBatch();

  // generate first PBFT block sample
  blk_hash_t prev_block_hash(0);
  uint64_t period = 1;
  addr_t beneficiary(987);

  vdf_sortition::Message msg1(1);
  vdf_sortition::VdfSortition vdf1(vdf_config, node_key.address(), vrf_sk, msg1);
  vdf1.computeVdfSolution(dag_genesis.toString());
  DagBlock blk1(dag_genesis, 1, {}, {}, vdf1);
  blk1.sign(sk);
  node1->getBlockManager()->insertBlock(blk1);

  PbftBlock pbft_block1(prev_block_hash, blk1.getHash(), period, beneficiary, node1->getSecretKey());
  db1->putFinalizedDagBlockHashesByAnchor(*batch, pbft_block1.getPivotDagBlockHash(), {pbft_block1.getPivotDagBlockHash()});
  std::vector<Vote> votes_for_pbft_blk1;
  votes_for_pbft_blk1.emplace_back(node1->getPbftManager()->generateVote(pbft_block1.getBlockHash(), cert_vote_type, 1, 3, prev_block_hash));
  std::cout << "Generate 1 vote for first PBFT block" << std::endl;
  // Add cert votes in DB
  db1->addPbftCertVotesToBatch(pbft_block1.getBlockHash(), votes_for_pbft_blk1, batch);
  // Add PBFT block in DB
  db1->addPbftBlockToBatch(pbft_block1, batch);
  // Update period_pbft_block in DB
  db1->addPbftBlockPeriodToBatch(period, pbft_block1.getBlockHash(), batch);
  // Update pbft chain
  pbft_chain1->updatePbftChain(pbft_block1.getBlockHash());
  // Update PBFT chain head block
  blk_hash_t pbft_chain_head_hash = pbft_chain1->getHeadHash();
  std::string pbft_chain_head_str = pbft_chain1->getJsonStr();
  db1->addPbftHeadToBatch(pbft_chain_head_hash, pbft_chain_head_str, batch);
  db1->commitWriteBatch(batch);
  int expect_pbft_chain_size = 1;
  EXPECT_EQ(node1->getPbftChain()->getPbftChainSize(), expect_pbft_chain_size);

  // generate second PBFT block sample
  prev_block_hash = pbft_block1.getBlockHash();

  vdf_sortition::Message msg2(2);
  vdf_sortition::VdfSortition vdf2(vdf_config, node_key.address(), vrf_sk, msg2);
  vdf2.computeVdfSolution(blk1.getHash().toString());
  DagBlock blk2(blk1.getHash(), 2, {}, {}, vdf2);
  blk2.sign(sk);
  node1->getBlockManager()->insertBlock(blk2);

  batch = db1->createWriteBatch();
  period = 2;
  beneficiary = addr_t(654);
  PbftBlock pbft_block2(prev_block_hash, blk2.getHash(), 2, beneficiary, node1->getSecretKey());
  db1->putFinalizedDagBlockHashesByAnchor(*batch, pbft_block2.getPivotDagBlockHash(), {pbft_block2.getPivotDagBlockHash()});

  std::vector<Vote> votes_for_pbft_blk2;
  votes_for_pbft_blk2.emplace_back(node1->getPbftManager()->generateVote(pbft_block2.getBlockHash(), cert_vote_type, 2, 3, prev_block_hash));
  std::cout << "Generate 1 vote for second PBFT block" << std::endl;
  // node1 put block2 into pbft chain and store into DB
  // Add cert votes in DB
  db1->addPbftCertVotesToBatch(pbft_block2.getBlockHash(), votes_for_pbft_blk2, batch);
  // Add PBFT block in DB
  db1->addPbftBlockToBatch(pbft_block2, batch);
  // Update period_pbft_block in DB
  db1->addPbftBlockPeriodToBatch(period, pbft_block2.getBlockHash(), batch);
  // Update pbft chain
  pbft_chain1->updatePbftChain(pbft_block2.getBlockHash());
  // Update PBFT chain head block
  pbft_chain_head_hash = pbft_chain1->getHeadHash();
  pbft_chain_head_str = pbft_chain1->getJsonStr();
  db1->addPbftHeadToBatch(pbft_chain_head_hash, pbft_chain_head_str, batch);
  db1->commitWriteBatch(batch);
  expect_pbft_chain_size = 2;
  EXPECT_EQ(node1->getPbftChain()->getPbftChainSize(), expect_pbft_chain_size);

  FullNode::Handle node2(node_cfgs[1], true);
  std::shared_ptr<Network> nw1 = node1->getNetwork();
  std::shared_ptr<Network> nw2 = node2->getNetwork();
  const int node_peers = 1;
  bool checkpoint_passed = false;
  const int timeout_val = 60;
  for (auto i = 0; i < timeout_val; i++) {
    // test timeout is 60 seconds
    if (nw1->getPeerCount() == node_peers && nw2->getPeerCount() == node_peers) {
      checkpoint_passed = true;
      break;
    }
    taraxa::thisThreadSleepForMilliSeconds(1000);
  }
  if (checkpoint_passed == false) {
    std::cout << "Timeout reached after " << timeout_val << " seconds..." << std::endl;
    ASSERT_EQ(node_peers, nw1->getPeerCount());
    ASSERT_EQ(node_peers, nw2->getPeerCount());
  }

  std::cout << "Waiting Sync for max 2 minutes..." << std::endl;
  for (int i = 0; i < 1200; i++) {
    if (node2->getPbftChain()->getPbftChainSize() == expect_pbft_chain_size) {
      break;
    }
    taraxa::thisThreadSleepForMilliSeconds(100);
  }
  EXPECT_EQ(node2->getPbftChain()->getPbftChainSize(), expect_pbft_chain_size);
  std::shared_ptr<PbftChain> pbft_chain2 = node2->getPbftChain();
  blk_hash_t second_pbft_block_hash = pbft_chain2->getLastPbftBlockHash();
  EXPECT_EQ(second_pbft_block_hash, pbft_block2.getBlockHash());
  blk_hash_t first_pbft_block_hash = pbft_chain2->getPbftBlockInChain(second_pbft_block_hash).getPrevBlockHash();
  EXPECT_EQ(first_pbft_block_hash, pbft_block1.getBlockHash());
}

TEST_F(NetworkTest, node_pbft_sync_without_enough_votes) {
  auto node_cfgs = make_node_cfgs(2);
  FullNode::Handle node1(node_cfgs[0], true);

  node1->getPbftManager()->stop();
  auto db1 = node1->getDB();
  auto pbft_chain1 = node1->getPbftChain();

  auto dag_genesis = node1->getConfig().chain.dag_genesis_block.getHash();
  auto sk = node1->getSecretKey();
  auto vrf_sk = node1->getVrfSecretKey();
  vdf_sortition::VdfConfig vdf_config(255, 0, 15, 10, 1500);
  auto batch = db1->createWriteBatch();

  // generate first PBFT block sample
  blk_hash_t prev_block_hash(0);
  uint64_t period = 1;
  addr_t beneficiary(876);

  vdf_sortition::Message msg1(1);
  vdf_sortition::VdfSortition vdf1(vdf_config, node_key.address(), vrf_sk, msg1);
  vdf1.computeVdfSolution(dag_genesis.toString());
  DagBlock blk1(dag_genesis, 1, {}, {}, vdf1);
  blk1.sign(sk);
  node1->getBlockManager()->insertBlock(blk1);

  PbftBlock pbft_block1(prev_block_hash, blk1.getHash(), period, beneficiary, node1->getSecretKey());
  db1->putFinalizedDagBlockHashesByAnchor(*batch, pbft_block1.getPivotDagBlockHash(), {pbft_block1.getPivotDagBlockHash()});
  std::vector<Vote> votes_for_pbft_blk1;
  votes_for_pbft_blk1.emplace_back(node1->getPbftManager()->generateVote(pbft_block1.getBlockHash(), cert_vote_type, 1, 3, prev_block_hash));
  std::cout << "Generate 1 vote for first PBFT block" << std::endl;
  // Add cert votes in DB
  db1->addPbftCertVotesToBatch(pbft_block1.getBlockHash(), votes_for_pbft_blk1, batch);
  // Add PBFT block in DB
  db1->addPbftBlockToBatch(pbft_block1, batch);
  // Update period_pbft_block in DB
  db1->addPbftBlockPeriodToBatch(period, pbft_block1.getBlockHash(), batch);
  // Update pbft chain
  pbft_chain1->updatePbftChain(pbft_block1.getBlockHash());
  // Update PBFT chain head block
  blk_hash_t pbft_chain_head_hash = pbft_chain1->getHeadHash();
  std::string pbft_chain_head_str = pbft_chain1->getJsonStr();
  db1->addPbftHeadToBatch(pbft_chain_head_hash, pbft_chain_head_str, batch);
  db1->commitWriteBatch(batch);
  int expect_pbft_chain_size = 1;
  EXPECT_EQ(node1->getPbftChain()->getPbftChainSize(), expect_pbft_chain_size);

  // generate second PBFT block sample
  prev_block_hash = pbft_block1.getBlockHash();
  period = 2;
  beneficiary = addr_t(543);
  vdf_sortition::Message msg2(2);
  vdf_sortition::VdfSortition vdf2(vdf_config, node_key.address(), vrf_sk, msg2);
  vdf2.computeVdfSolution(blk1.getHash().toString());
  DagBlock blk2(blk1.getHash(), 2, {}, {}, vdf2);
  blk2.sign(sk);
  node1->getBlockManager()->insertBlock(blk2);

  batch = db1->createWriteBatch();
  period = 2;
  beneficiary = addr_t(654);

  PbftBlock pbft_block2(prev_block_hash, blk2.getHash(), period, beneficiary, node1->getSecretKey());
  db1->putFinalizedDagBlockHashesByAnchor(*batch, pbft_block2.getPivotDagBlockHash(), {pbft_block2.getPivotDagBlockHash()});
  std::cout << "There are no votes for the second PBFT block" << std::endl;
  // node1 put block2 into pbft chain and no votes store into DB
  // (malicious player)
  // Add PBFT block in DB
  db1->addPbftBlockToBatch(pbft_block2, batch);
  // Update period_pbft_block in DB
  db1->addPbftBlockPeriodToBatch(period, pbft_block2.getBlockHash(), batch);
  // Update pbft chain
  pbft_chain1->updatePbftChain(pbft_block2.getBlockHash());
  // Update PBFT chain head block
  pbft_chain_head_hash = pbft_chain1->getHeadHash();
  pbft_chain_head_str = pbft_chain1->getJsonStr();
  db1->addPbftHeadToBatch(pbft_chain_head_hash, pbft_chain_head_str, batch);
  db1->commitWriteBatch(batch);
  expect_pbft_chain_size = 2;
  EXPECT_EQ(node1->getPbftChain()->getPbftChainSize(), expect_pbft_chain_size);

  FullNode::Handle node2(node_cfgs[1], true);
  std::shared_ptr<Network> nw1 = node1->getNetwork();
  std::shared_ptr<Network> nw2 = node2->getNetwork();
  const int node_peers = 1;
  bool checkpoint_passed = false;
  const int timeout_val = 60;
  for (auto i = 0; i < timeout_val; i++) {
    // test timeout is 60 seconds
    if (nw1->getPeerCount() == node_peers && nw2->getPeerCount() == node_peers) {
      checkpoint_passed = true;
      break;
    }
    taraxa::thisThreadSleepForMilliSeconds(1000);
  }
  if (checkpoint_passed == false) {
    std::cout << "Timeout reached after " << timeout_val << " seconds..." << std::endl;
    ASSERT_EQ(node_peers, nw1->getPeerCount());
    ASSERT_EQ(node_peers, nw2->getPeerCount());
  }

  std::cout << "Waiting Sync for max 1 minutes..." << std::endl;
  int sync_pbft_chain_size = 1;
  for (int i = 0; i < 600; i++) {
    if (node2->getPbftChain()->getPbftChainSize() >= sync_pbft_chain_size) {
      break;
    }
    taraxa::thisThreadSleepForMilliSeconds(100);
  }
  EXPECT_EQ(node2->getPbftChain()->getPbftChainSize(), sync_pbft_chain_size);
  std::shared_ptr<PbftChain> pbft_chain2 = node2->getPbftChain();
  blk_hash_t last_pbft_block_hash = pbft_chain2->getLastPbftBlockHash();
  EXPECT_EQ(last_pbft_block_hash, pbft_block1.getBlockHash());
}

// Test PBFT next votes bundle network propagation
TEST_F(NetworkTest, pbft_next_votes_bundle_sync) {
  auto node_cfgs = make_node_cfgs<5>(2);
  FullNode::Handle node1(node_cfgs[0], true);
  // Stop PBFT manager, that will place vote
  std::shared_ptr<PbftManager> pbft_mgr1 = node1->getPbftManager();
  pbft_mgr1->stop();
  // Generate 3 next votes
  std::vector<Vote> next_votes;
  for (auto i = 0; i < 3; i++) {
    blk_hash_t propose_pbft_block_hash(i + 1);
    blk_hash_t last_pbft_block_hash(i);
    PbftVoteTypes type = next_vote_type;
    uint64_t round = 10;
    size_t step = 21;
    Vote vote = node1->getPbftManager()->generateVote(propose_pbft_block_hash, type, round, step, last_pbft_block_hash);
    next_votes.emplace_back(vote);
  }
  // Update next votes bundle and set PBFT round
  pbft_mgr1->updateNextVotesForRound(next_votes);
  pbft_mgr1->setPbftRound(11);  // Make sure node2 round less than node1

  // Start node2
  FullNode::Handle node2(node_cfgs[1], true);
  std::shared_ptr<Network> nw1 = node1->getNetwork();
  std::shared_ptr<Network> nw2 = node2->getNetwork();
  // Wait node1 and node2 connect to each other
  int node_peers = 1;
  for (int i = 0; i < 300; i++) {
    // test timeout is 30 seconds
    if (nw1->getPeerCount() == node_peers && nw2->getPeerCount() == node_peers) {
      break;
    }
    taraxa::thisThreadSleepForMilliSeconds(100);
  }
  EXPECT_EQ(nw1->getPeerCount(), 1);
  EXPECT_EQ(nw2->getPeerCount(), 1);
  // Stop PBFT manager, that will place vote
  std::shared_ptr<PbftManager> pbft_mgr2 = node2->getPbftManager();
  pbft_mgr2->stop();
  node2->getVoteManager()->clearUnverifiedVotesTable();
  // Wait 6 PBFT lambda time for sending network status, 2000 / 5 * 6 = 2400
  taraxa::thisThreadSleepForMilliSeconds(2400);
  // node2 sync next votes bundle from node1
  uint64_t node2_pbft_round = 10;
  nw2->getTaraxaCapability()->syncPbftNextVotes(node2_pbft_round);
  // Waiting node2 get next votes bundle from node1
  taraxa::thisThreadSleepForMilliSeconds(100);
  size_t node2_vote_queue_size = node2->getVoteManager()->getUnverifiedVotesSize();
  EXPECT_EQ(node2_vote_queue_size, next_votes.size());
}

// Test creates a DAG on one node and verifies
// that the second node syncs with it and that the resulting
// DAG on the other end is the same
// Unlike the previous tests, this DAG contains blocks with transactions
// and verifies that the sync containing transactions is successful
TEST_F(NetworkTest, node_sync_with_transactions) {
  auto node_cfgs = make_node_cfgs<5>(2);
  FullNode::Handle node1(node_cfgs[0], true);

  std::vector<DagBlock> blks;
  // Generate DAG blocks
  auto dag_genesis = node1->getConfig().chain.dag_genesis_block.getHash();
  auto sk = node1->getSecretKey();
  auto vrf_sk = node1->getVrfSecretKey();
  vdf_sortition::VdfConfig vdf_config(255, 0, 15, 10, 1500);
  auto propose_level = 1;
  vdf_sortition::Message msg1(propose_level);
  vdf_sortition::VdfSortition vdf1(vdf_config, node_key.address(), vrf_sk, msg1);
  vdf1.computeVdfSolution(dag_genesis.toString());
  DagBlock blk1(dag_genesis, propose_level, {}, {g_signed_trx_samples[0].getHash(), g_signed_trx_samples[1].getHash()}, vdf1);
  blk1.sign(sk);
  std::vector<Transaction> tr1({g_signed_trx_samples[0], g_signed_trx_samples[1]});

  propose_level = 2;
  vdf_sortition::Message msg2(propose_level);
  vdf_sortition::VdfSortition vdf2(vdf_config, node_key.address(), vrf_sk, msg2);
  vdf2.computeVdfSolution(blk1.getHash().toString());
  DagBlock blk2(blk1.getHash(), propose_level, {}, {g_signed_trx_samples[2].getHash()}, vdf2);
  blk2.sign(sk);
  std::vector<Transaction> tr2({g_signed_trx_samples[2]});

  propose_level = 3;
  vdf_sortition::Message msg3(propose_level);
  vdf_sortition::VdfSortition vdf3(vdf_config, node_key.address(), vrf_sk, msg3);
  vdf3.computeVdfSolution(blk2.getHash().toString());
  DagBlock blk3(blk2.getHash(), propose_level, {}, {}, vdf3);
  blk3.sign(sk);
  std::vector<Transaction> tr3;

  propose_level = 4;
  vdf_sortition::Message msg4(propose_level);
  vdf_sortition::VdfSortition vdf4(vdf_config, node_key.address(), vrf_sk, msg4);
  vdf4.computeVdfSolution(blk3.getHash().toString());
  DagBlock blk4(blk3.getHash(), propose_level, {}, {g_signed_trx_samples[3].getHash(), g_signed_trx_samples[4].getHash()}, vdf4);
  blk4.sign(sk);
  std::vector<Transaction> tr4({g_signed_trx_samples[3], g_signed_trx_samples[4]});

  propose_level = 5;
  vdf_sortition::Message msg5(propose_level);
  vdf_sortition::VdfSortition vdf5(vdf_config, node_key.address(), vrf_sk, msg5);
  vdf5.computeVdfSolution(blk4.getHash().toString());
  DagBlock blk5(
      blk4.getHash(), propose_level, {},
      {g_signed_trx_samples[5].getHash(), g_signed_trx_samples[6].getHash(), g_signed_trx_samples[7].getHash(), g_signed_trx_samples[8].getHash()},
      vdf5);
  blk5.sign(sk);
  std::vector<Transaction> tr5({g_signed_trx_samples[5], g_signed_trx_samples[6], g_signed_trx_samples[7], g_signed_trx_samples[8]});

  propose_level = 6;
  vdf_sortition::Message msg6(propose_level);
  vdf_sortition::VdfSortition vdf6(vdf_config, node_key.address(), vrf_sk, msg6);
  vdf6.computeVdfSolution(blk5.getHash().toString());
  DagBlock blk6(blk5.getHash(), propose_level, {blk4.getHash(), blk3.getHash()}, {g_signed_trx_samples[9].getHash()}, vdf6);
  blk6.sign(sk);
  std::vector<Transaction> tr6({g_signed_trx_samples[9]});

  node1->getBlockManager()->insertBroadcastedBlockWithTransactions(blk6, tr6);
  node1->getBlockManager()->insertBroadcastedBlockWithTransactions(blk5, tr5);
  node1->getBlockManager()->insertBroadcastedBlockWithTransactions(blk4, tr4);
  node1->getBlockManager()->insertBroadcastedBlockWithTransactions(blk3, tr3);
  node1->getBlockManager()->insertBroadcastedBlockWithTransactions(blk2, tr2);
  node1->getBlockManager()->insertBroadcastedBlockWithTransactions(blk1, tr1);

  // To make sure blocks are stored before starting node 2
  taraxa::thisThreadSleepForMilliSeconds(1000);

  FullNode::Handle node2(node_cfgs[1], true);
  std::cout << "Waiting Sync for up to 20000 milliseconds ..." << std::endl;
  for (int i = 0; i < 40; i++) {
    taraxa::thisThreadSleepForMilliSeconds(1000);
    if (node2->getDagManager()->getNumVerticesInDag().first == 7 && node2->getDagManager()->getNumEdgesInDag().first == 8) break;
  }

  // node1->drawGraph("dot.txt");
  EXPECT_EQ(node1->getNumReceivedBlocks(), 6);
  EXPECT_EQ(node1->getDagManager()->getNumVerticesInDag().first, 7);
  EXPECT_EQ(node1->getDagManager()->getNumEdgesInDag().first, 8);

  EXPECT_EQ(node2->getNumReceivedBlocks(), 6);
  EXPECT_EQ(node2->getDagManager()->getNumVerticesInDag().first, 7);
  EXPECT_EQ(node2->getDagManager()->getNumEdgesInDag().first, 8);
}

// Test creates a complex DAG on one node and verifies
// that the second node syncs with it and that the resulting
// DAG on the other end is the same
TEST_F(NetworkTest, node_sync2) {
  auto node_cfgs = make_node_cfgs<5>(2);
  FullNode::Handle node1(node_cfgs[0], true);

  std::vector<DagBlock> blks;
  // Generate DAG blocks
  auto dag_genesis = node1->getConfig().chain.dag_genesis_block.getHash();
  auto sk = node1->getSecretKey();
  auto vrf_sk = node1->getVrfSecretKey();
  vdf_sortition::VdfConfig vdf_config(255, 0, 15, 10, 1500);
  auto transactions = samples::createSignedTrxSamples(0, NUM_TRX2, sk);
  // DAG block1
  auto propose_level = 1;
  vdf_sortition::Message msg1(propose_level);
  vdf_sortition::VdfSortition vdf1(vdf_config, node_key.address(), vrf_sk, msg1);
  vdf1.computeVdfSolution(dag_genesis.toString());
  DagBlock blk1(dag_genesis, propose_level, {}, {transactions[0].getHash(), transactions[1].getHash()}, vdf1);
  blk1.sign(sk);
  std::vector<Transaction> tr1({transactions[0], transactions[1]});
  // DAG block2
  propose_level = 1;
  vdf_sortition::Message msg2(propose_level);
  vdf_sortition::VdfSortition vdf2(vdf_config, node_key.address(), vrf_sk, msg2);
  vdf2.computeVdfSolution(dag_genesis.toString());
  DagBlock blk2(dag_genesis, propose_level, {}, {transactions[2].getHash(), transactions[3].getHash()}, vdf2);
  blk2.sign(sk);
  std::vector<Transaction> tr2({transactions[2], transactions[3]});
  // DAG block3
  propose_level = 2;
  vdf_sortition::Message msg3(propose_level);
  vdf_sortition::VdfSortition vdf3(vdf_config, node_key.address(), vrf_sk, msg3);
  vdf3.computeVdfSolution(blk1.getHash().toString());
  DagBlock blk3(blk1.getHash(), propose_level, {}, {transactions[4].getHash(), transactions[5].getHash()}, vdf3);
  blk3.sign(sk);
  std::vector<Transaction> tr3({transactions[4], transactions[5]});
  // DAG block4
  propose_level = 3;
  vdf_sortition::Message msg4(propose_level);
  vdf_sortition::VdfSortition vdf4(vdf_config, node_key.address(), vrf_sk, msg4);
  vdf4.computeVdfSolution(blk3.getHash().toString());
  DagBlock blk4(blk3.getHash(), propose_level, {}, {transactions[6].getHash(), transactions[7].getHash()}, vdf4);
  blk4.sign(sk);
  std::vector<Transaction> tr4({transactions[6], transactions[7]});
  // DAG block5
  propose_level = 2;
  vdf_sortition::Message msg5(propose_level);
  vdf_sortition::VdfSortition vdf5(vdf_config, node_key.address(), vrf_sk, msg5);
  vdf5.computeVdfSolution(blk2.getHash().toString());
  DagBlock blk5(blk2.getHash(), propose_level, {}, {transactions[8].getHash(), transactions[9].getHash()}, vdf5);
  blk5.sign(sk);
  std::vector<Transaction> tr5({transactions[8], transactions[9]});
  // DAG block6
  propose_level = 2;
  vdf_sortition::Message msg6(propose_level);
  vdf_sortition::VdfSortition vdf6(vdf_config, node_key.address(), vrf_sk, msg6);
  vdf6.computeVdfSolution(blk1.getHash().toString());
  DagBlock blk6(blk1.getHash(), propose_level, {}, {transactions[10].getHash(), transactions[11].getHash()}, vdf6);
  blk6.sign(sk);
  std::vector<Transaction> tr6({transactions[10], transactions[11]});
  // DAG block7
  propose_level = 3;
  vdf_sortition::Message msg7(propose_level);
  vdf_sortition::VdfSortition vdf7(vdf_config, node_key.address(), vrf_sk, msg7);
  vdf7.computeVdfSolution(blk6.getHash().toString());
  DagBlock blk7(blk6.getHash(), propose_level, {}, {transactions[12].getHash(), transactions[13].getHash()}, vdf7);
  blk7.sign(sk);
  std::vector<Transaction> tr7({transactions[12], transactions[13]});
  // DAG block8
  propose_level = 4;
  vdf_sortition::Message msg8(propose_level);
  vdf_sortition::VdfSortition vdf8(vdf_config, node_key.address(), vrf_sk, msg8);
  vdf8.computeVdfSolution(blk1.getHash().toString());
  DagBlock blk8(blk1.getHash(), propose_level, {blk7.getHash()}, {transactions[14].getHash(), transactions[15].getHash()}, vdf8);
  blk8.sign(sk);
  std::vector<Transaction> tr8({transactions[14], transactions[15]});
  // DAG block9
  propose_level = 2;
  vdf_sortition::Message msg9(propose_level);
  vdf_sortition::VdfSortition vdf9(vdf_config, node_key.address(), vrf_sk, msg9);
  vdf9.computeVdfSolution(blk1.getHash().toString());
  DagBlock blk9(blk1.getHash(), propose_level, {}, {transactions[16].getHash(), transactions[17].getHash()}, vdf9);
  blk9.sign(sk);
  std::vector<Transaction> tr9({transactions[16], transactions[17]});
  // DAG block10
  propose_level = 5;
  vdf_sortition::Message msg10(propose_level);
  vdf_sortition::VdfSortition vdf10(vdf_config, node_key.address(), vrf_sk, msg10);
  vdf10.computeVdfSolution(blk8.getHash().toString());
  DagBlock blk10(blk8.getHash(), propose_level, {}, {transactions[18].getHash(), transactions[19].getHash()}, vdf10);
  blk10.sign(sk);
  std::vector<Transaction> tr10({transactions[18], transactions[19]});
  // DAG block11
  propose_level = 3;
  vdf_sortition::Message msg11(propose_level);
  vdf_sortition::VdfSortition vdf11(vdf_config, node_key.address(), vrf_sk, msg11);
  vdf11.computeVdfSolution(blk3.getHash().toString());
  DagBlock blk11(blk3.getHash(), propose_level, {}, {transactions[20].getHash(), transactions[21].getHash()}, vdf11);
  blk11.sign(sk);
  std::vector<Transaction> tr11({transactions[20], transactions[21]});
  // DAG block12
  propose_level = 3;
  vdf_sortition::Message msg12(propose_level);
  vdf_sortition::VdfSortition vdf12(vdf_config, node_key.address(), vrf_sk, msg12);
  vdf12.computeVdfSolution(blk5.getHash().toString());
  DagBlock blk12(blk5.getHash(), propose_level, {}, {transactions[22].getHash(), transactions[23].getHash()}, vdf12);
  blk12.sign(sk);
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
    node1->getBlockManager()->insertBroadcastedBlockWithTransactions(blks[i], trxs[i]);
  }

  taraxa::thisThreadSleepForMilliSeconds(200);

  FullNode::Handle node2(node_cfgs[1], true);
  std::cout << "Waiting Sync for up to 40000 milliseconds ..." << std::endl;
  for (int i = 0; i < 400; i++) {
    taraxa::thisThreadSleepForMilliSeconds(100);
    if (node2->getDagManager()->getNumVerticesInDag().first == 13 && node2->getDagManager()->getNumEdgesInDag().first == 13) break;
  }

  // node1->drawGraph("dot.txt");
  EXPECT_EQ(node1->getNumReceivedBlocks(), blks.size());
  EXPECT_EQ(node1->getDagManager()->getNumVerticesInDag().first, 13);
  EXPECT_EQ(node1->getDagManager()->getNumEdgesInDag().first, 13);

  EXPECT_EQ(node2->getNumReceivedBlocks(), blks.size());
  EXPECT_EQ(node2->getDagManager()->getNumVerticesInDag().first, 13);
  EXPECT_EQ(node2->getDagManager()->getNumEdgesInDag().first, 13);
}

// Test creates new transactions on one node and verifies
// that the second node receives the transactions
TEST_F(NetworkTest, node_transaction_sync) {
  auto node_cfgs = make_node_cfgs(2);
  auto nodes = launch_nodes(node_cfgs);
  auto& node1 = nodes[0];
  auto& node2 = nodes[1];

  std::vector<taraxa::bytes> transactions;
  for (auto const& t : *g_signed_trx_samples) {
    transactions.emplace_back(*t.rlp());
  }

  node1->getTransactionManager()->insertBroadcastedTransactions(transactions);

  std::cout << "Waiting Sync for 2000 milliseconds ..." << std::endl;
  taraxa::thisThreadSleepForMilliSeconds(2000);

  for (auto const& t : *g_signed_trx_samples) {
    EXPECT_TRUE(node2->getTransactionManager()->getTransaction(t.getHash()) != nullptr);
    if (node2->getTransactionManager()->getTransaction(t.getHash()) != nullptr) {
      EXPECT_EQ(t, node2->getTransactionManager()->getTransaction(t.getHash())->first);
    }
  }
}

// Test creates multiple nodes and creates new transactions in random time
// intervals on randomly selected nodes It verifies that the blocks created from
// these transactions which get created on random nodes are synced and the
// resulting DAG is the same on all nodes
TEST_F(NetworkTest, node_full_sync) {
  constexpr auto numberOfNodes = 5;
  auto node_cfgs = make_node_cfgs(numberOfNodes);
  auto nodes = launch_nodes(slice(node_cfgs, 0, numberOfNodes - 1));

  std::random_device dev;
  std::mt19937 rng(dev());
  std::uniform_int_distribution<std::mt19937::result_type> distTransactions(1, 200);
  std::uniform_int_distribution<std::mt19937::result_type> distNodes(0, numberOfNodes - 2);  // range [0, 3]

  int counter = 0;
  std::vector<Transaction> ts;
  // Generate 50 transactions
  for (auto i = 0; i < 50; ++i) {
    ts.push_back(samples::TX_GEN->getWithRandomUniqueSender());
  }
  for (auto i = 0; i < 50; ++i) {
    std::vector<taraxa::bytes> transactions;
    transactions.emplace_back(*ts[i].rlp());
    nodes[distNodes(rng)]->getTransactionManager()->insertBroadcastedTransactions(transactions);
    thisThreadSleepForMilliSeconds(distTransactions(rng));
    counter++;
  }
  ASSERT_EQ(counter, 50);  // 50 transactions

  std::cout << "Waiting Sync ..." << std::endl;
  wait({120s, 500ms}, [&](auto& ctx) {
    // Check 4 nodes DAG syncing
    for (int j = 1; j < numberOfNodes - 1; j++) {
      WAIT_EXPECT_EQ(ctx, nodes[j]->getDagManager()->getNumVerticesInDag().first, nodes[0]->getDagManager()->getNumVerticesInDag().first);
    }
  });

  // Bootstrapping node5 join the network
  nodes.emplace_back(FullNode::Handle(node_cfgs[numberOfNodes - 1], true));
  EXPECT_TRUE(wait_connect(nodes));

  std::cout << "Waiting Sync..." << std::endl;
  wait({240s, 1000ms}, [&](auto& ctx) {
    // Check 4 nodes DAG syncing
    for (int j = 1; j < numberOfNodes; j++) {
      WAIT_EXPECT_EQ(ctx, nodes[j]->getDagManager()->getNumVerticesInDag().first, nodes[0]->getDagManager()->getNumVerticesInDag().first);
      ctx.fail_if(!nodes[j]->getNetwork()->isSynced());
    }
  });

  EXPECT_GT(nodes[0]->getDagManager()->getNumVerticesInDag().first, 0);
  for (int i = 0; i < numberOfNodes; i++) {
    std::cout << "Index i " << i << std::endl;
    EXPECT_GT(nodes[i]->getDagManager()->getNumVerticesInDag().first, 0);
    EXPECT_EQ(nodes[i]->getDagManager()->getNumVerticesInDag().first, nodes[0]->getDagManager()->getNumVerticesInDag().first);
    EXPECT_EQ(nodes[i]->getDagManager()->getNumVerticesInDag().first, nodes[i]->getDB()->getNumDagBlocks());
    EXPECT_EQ(nodes[i]->getDagManager()->getNumEdgesInDag().first, nodes[0]->getDagManager()->getNumEdgesInDag().first);
    EXPECT_TRUE(nodes[i]->getNetwork()->isSynced());
  }

  // Write any DAG diff
  for (int i = 1; i < numberOfNodes; i++) {
    uint64_t level = 1;
    while (true) {
      auto blocks1 = nodes[0]->getDB()->getDagBlocksAtLevel(level, 1);
      auto blocks2 = nodes[i]->getDB()->getDagBlocksAtLevel(level, 1);
      if (blocks1.size() != blocks2.size()) {
        std::cout << "DIFF at level %lu: " << level << std::endl;
        for (auto b : blocks1) printf(" %s", b->getHash().toString().c_str());
        printf("\n");
        for (auto b : blocks2) printf(" %s", b->getHash().toString().c_str());
        printf("\n");
      }
      if (blocks1.size() == 0 && blocks2.size() == 0) break;
      level++;
    }
  }

  // This checks for any duplicate transaction in consecutive blocks
  std::map<blk_hash_t, std::set<trx_hash_t>> trxHist;
  uint64_t level = 1;
  while (true) {
    auto blocks1 = nodes[0]->getDB()->getDagBlocksAtLevel(level, 1);
    for (auto b : blocks1) {
      for (auto t : trxHist[b->getPivot()]) trxHist[b->getHash()].insert(t);
      for (auto tip : b->getTips()) {
        for (auto t : trxHist[tip]) trxHist[b->getHash()].insert(t);
      }
      for (auto t : b->getTrxs()) {
        if (trxHist[b->getHash()].count(t) > 0) {
          printf("FOUND DUPLICATE TRANSACTION %s\n", t.toString().c_str());
          EXPECT_TRUE(false);
        }
        trxHist[b->getHash()].insert(t);
      }
    }
    if (blocks1.size() == 0) break;
    level++;
  }
}

}  // namespace taraxa::core_tests

using namespace taraxa;
int main(int argc, char** argv) {
  taraxa::static_init();
  LoggingConfig logging;
  logging.verbosity = taraxa::VerbosityError;
  addr_t node_addr;
  setupLoggingConfiguration(node_addr, logging);
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}