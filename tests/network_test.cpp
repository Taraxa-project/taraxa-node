#include "network/network.hpp"

#include <gtest/gtest.h>

#include <atomic>
#include <iostream>
#include <vector>

#include "common/static_init.hpp"
#include "consensus/block_proposer.hpp"
#include "consensus/pbft_manager.hpp"
#include "dag/dag.hpp"
#include "logger/log.hpp"
#include "util/lazy.hpp"
#include "util_test/samples.hpp"
#include "util_test/util.hpp"

namespace taraxa::core_tests {

const unsigned NUM_TRX = 10;
auto g_secret = Lazy([] {
  return dev::Secret("3800b2875669d9b2053c1aff9224ecfdc411423aac5b5a73d7a45ced1c3b9dcd",
                     dev::Secret::ConstructFromStringType::FromHex);
});
auto node_key = dev::KeyPair(g_secret);

auto g_trx_samples = Lazy([] { return samples::createMockTrxSamples(0, NUM_TRX); });
auto g_signed_trx_samples = Lazy([] { return samples::createSignedTrxSamples(0, NUM_TRX, g_secret); });

const unsigned NUM_TRX2 = 200;
auto g_secret2 = Lazy([] {
  return dev::Secret("3800b2875669d9b2053c1aff9224ecfdc411423aac5b5a73d7a45ced1c3b9dcd",
                     dev::Secret::ConstructFromStringType::FromHex);
});
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
  std::unique_ptr<Network> nw1(new taraxa::Network(g_conf1->network));
  std::unique_ptr<Network> nw2(new taraxa::Network(g_conf2->network));

  nw1->start();
  nw2->start();
  DagBlock blk(blk_hash_t(1111), 0, {blk_hash_t(222), blk_hash_t(333), blk_hash_t(444)},
               {g_signed_trx_samples[0].getHash(), g_signed_trx_samples[1].getHash()}, sig_t(7777), blk_hash_t(888),
               addr_t(999));

  std::vector<taraxa::bytes> transactions;
  transactions.emplace_back(*g_signed_trx_samples[0].rlp());
  transactions.emplace_back(*g_signed_trx_samples[1].rlp());
  nw2->onNewTransactions(transactions);

  taraxa::thisThreadSleepForSeconds(1);

  for (auto i = 0; i < 1; ++i) {
    nw2->sendBlock(nw1->getNodeId(), blk);
  }

  std::cout << "Waiting packages for 10 seconds ..." << std::endl;

  for (int i = 0; i < 100; i++) {
    if (nw1->getReceivedBlocksCount()) break;
    taraxa::thisThreadSleepForMilliSeconds(100);
  }
  nw2 = nullptr;
  unsigned long long num_received = nw1->getReceivedBlocksCount();
  nw1 = nullptr;
  ASSERT_EQ(1, num_received);
}

// Test creates two Network setup and verifies sending blocks
// between is successfull
TEST_F(NetworkTest, transfer_lot_of_blocks) {
  auto node_cfgs = make_node_cfgs<20, true>(2);
  auto nodes = launch_nodes(node_cfgs);

  std::vector<std::shared_ptr<DagBlock>> dag_blocks;
  std::vector<trx_hash_t> trx_hashes;
  std::vector<taraxa::bytes> trx_bytes;

  // creating a lot of trxs
  auto trxs = samples::createSignedTrxSamples(0, 1500, g_secret);
  for (const auto& trx : trxs) {
    trx_bytes.emplace_back(*trx.rlp());
    trx_hashes.push_back(trx.getHash());
  }

  // creating lot of non valid blocks just for size
  for (int i = 0; i < 100; ++i) {
    DagBlock blk(blk_hash_t(1111 + i), 0, {blk_hash_t(222 + i), blk_hash_t(333 + i), blk_hash_t(444 + i)}, trx_hashes,
                 sig_t(7777 + i), blk_hash_t(888 + i), addr_t(999 + i));
    dag_blocks.emplace_back(std::make_shared<DagBlock>(blk));
  }

  // add one valid as last
  auto dag_genesis = nodes[1]->getConfig().chain.dag_genesis_block.getHash();
  vdf_sortition::VdfConfig vdf_config(node_cfgs[1].chain.vdf);
  vdf_sortition::VdfSortition vdf(vdf_config, nodes[1]->getVrfSecretKey(), getRlpBytes(1));
  vdf.computeVdfSolution(vdf_config, dag_genesis.asBytes());
  DagBlock blk(dag_genesis, 1, {}, {samples::createSignedTrxSamples(0, 1, g_secret)[0].getHash()}, vdf,
               nodes[1]->getSecretKey());

  auto block_hash = blk.getHash();
  dag_blocks.emplace_back(std::make_shared<DagBlock>(blk));

  nodes[0]->getNetwork()->onNewTransactions(std::move(trx_bytes));
  taraxa::thisThreadSleepForSeconds(1);
  nodes[0]->getNetwork()->sendBlocks(nodes[1]->getNetwork()->getNodeId(), std::move(dag_blocks));

  std::cout << "Waiting Sync ..." << std::endl;
  wait({5s, 300ms},
       [&](auto& ctx) { WAIT_EXPECT_NE(ctx, nodes[1]->getDagBlockManager()->getDagBlock(block_hash), nullptr) });
}

TEST_F(NetworkTest, send_pbft_block) {
  auto node_cfgs = make_node_cfgs<5>(2);
  auto nodes = launch_nodes(node_cfgs);
  auto nw1 = nodes[0]->getNetwork();
  auto nw2 = nodes[1]->getNetwork();

  auto pbft_block = make_simple_pbft_block(blk_hash_t(1), 2);
  uint64_t chain_size = 111;

  nw2->sendPbftBlock(nw1->getNodeId(), pbft_block, chain_size);

  auto node2_id = nw2->getNodeId();
  EXPECT_HAPPENS({10s, 200ms},
                 [&](auto& ctx) { WAIT_EXPECT_EQ(ctx, nw1->getPeer(node2_id)->pbft_chain_size_, chain_size) });
}

TEST_F(NetworkTest, sync_large_pbft_block) {
  const uint32_t MAX_PACKET_SIZE = 15 * 1024 * 1024;  // 15 MB -> 15 * 1024 * 1024 B
  auto node_cfgs = make_node_cfgs<5>(2);
  // Increase lambda to avoid pbft block created too soon
  node_cfgs[0].chain.pbft.lambda_ms_min = 300;

  // Create large transactions with 10k dummy data
  bytes dummy_10k_data(100000, 0);
  auto signed_trxs = samples::createSignedTrxSamples(0, 250, g_secret2, dummy_10k_data);
  auto nodes = launch_nodes({node_cfgs[0]});
  auto nw1 = nodes[0]->getNetwork();

  size_t last_dag_level = 0;
  for (size_t i = 0; i < signed_trxs.size(); i++) {
    // Splits transactions into multiple dag blocks. Size of dag blocks should be about 5MB for 50 10k transactions
    if ((i + 1) % 50 == 0) {
      wait({10s, 10ms}, [&](auto& ctx) { ctx.fail_if(nodes[0]->getDagManager()->getMaxLevel() > last_dag_level); });
      last_dag_level++;
    }
    nodes[0]->getTransactionManager()->insertTransaction(signed_trxs[i], true, false);
  }
  // Wait untill pbft block is created
  wait({30s, 100ms}, [&](auto& ctx) { ctx.fail_if(nodes[0]->getPbftChain()->getPbftChainSize() == 0); });
  EXPECT_GT(nodes[0]->getPbftChain()->getPbftChainSize(), 0);

  // Verify that a block over MAX_PACKET_SIZE is created
  auto pbft_blocks = nodes[0]->getPbftChain()->getPbftBlocks(1, 1);
  size_t total_size = pbft_blocks[0].rlp().size();
  auto blocks = nodes[0]->getDB()->getFinalizedDagBlockHashesByAnchor(pbft_blocks[0].pbft_blk->getPivotDagBlockHash());
  for (auto b : blocks) {
    auto block = nodes[0]->getDB()->getDagBlock(b);
    total_size += block->rlp(true).size();
    for (auto t : block->getTrxs()) {
      auto trx = nodes[0]->getDB()->getTransaction(t);
      total_size += trx->rlp(true)->size();
    }
  }
  EXPECT_GT(total_size, MAX_PACKET_SIZE);

  // Launch second node and verify that the large pbft block is synced
  auto nodes2 = launch_nodes({node_cfgs[1]});
  wait({30s, 100ms}, [&](auto& ctx) { ctx.fail_if(nodes2[0]->getPbftChain()->getPbftChainSize() == 0); });
  EXPECT_GT(nodes2[0]->getPbftChain()->getPbftChainSize(), 0);

  auto pbft_blocks1 = nodes[0]->getPbftChain()->getPbftBlocks(1, 1);
  auto pbft_blocks2 = nodes2[0]->getPbftChain()->getPbftBlocks(1, 1);
  EXPECT_EQ(pbft_blocks1[0].rlp(), pbft_blocks2[0].rlp());
}

// Test creates two Network setup and verifies sending transaction
// between is successfull
TEST_F(NetworkTest, transfer_transaction) {
  auto nw1 = make_unique<Network>(g_conf1->network);
  auto nw2 = make_unique<Network>(g_conf2->network);
  nw1->start();
  nw2->start();

  EXPECT_HAPPENS({10s, 200ms}, [&](auto& ctx) {
    WAIT_EXPECT_EQ(ctx, nw1->getPeerCount(), 1)
    WAIT_EXPECT_EQ(ctx, nw2->getPeerCount(), 1)
  });
  auto nw1_nodeid = nw1->getNodeId();
  auto nw2_nodeid = nw2->getNodeId();
  EXPECT_NE(nw1->getPeer(nw2_nodeid), nullptr);
  EXPECT_NE(nw2->getPeer(nw1_nodeid), nullptr);

  std::vector<taraxa::bytes> transactions;
  transactions.push_back(*g_signed_trx_samples[0].rlp());
  transactions.push_back(*g_signed_trx_samples[1].rlp());
  transactions.push_back(*g_signed_trx_samples[2].rlp());

  nw2->sendTransactions(nw1_nodeid, transactions);

  EXPECT_HAPPENS({2s, 200ms}, [&](auto& ctx) { WAIT_EXPECT_EQ(ctx, nw1->getReceivedTransactionsCount(), 3) });
}

// Test verifies saving network to a file and restoring it from a file
// is successfull. Once restored from the file it is able to reestablish
// connections even with boot nodes down
TEST_F(NetworkTest, save_network) {
  std::filesystem::remove_all("/tmp/nw2");
  std::filesystem::remove_all("/tmp/nw3");
  auto key2 = dev::KeyPair::create();
  auto key3 = dev::KeyPair::create();
  {
    std::shared_ptr<Network> nw1(new taraxa::Network(g_conf1->network));
    std::shared_ptr<Network> nw2(new taraxa::Network(g_conf2->network, "/tmp/nw2", key2));
    std::shared_ptr<Network> nw3(new taraxa::Network(g_conf3->network, "/tmp/nw3", key3));

    nw1->start();
    nw2->start();
    nw3->start();

    EXPECT_HAPPENS({120s, 500ms}, [&](auto& ctx) {
      WAIT_EXPECT_EQ(ctx, nw1->getPeerCount(), 2)
      WAIT_EXPECT_EQ(ctx, nw2->getPeerCount(), 2)
      WAIT_EXPECT_EQ(ctx, nw3->getPeerCount(), 2)
    });
  }

  std::shared_ptr<Network> nw2(new taraxa::Network(g_conf2->network, "/tmp/nw2", key2));
  std::shared_ptr<Network> nw3(new taraxa::Network(g_conf3->network, "/tmp/nw3", key3));
  nw2->start();
  nw3->start();

  EXPECT_HAPPENS({120s, 500ms}, [&](auto& ctx) {
    WAIT_EXPECT_EQ(ctx, nw2->getPeerCount(), 1)
    WAIT_EXPECT_EQ(ctx, nw3->getPeerCount(), 1)
  });
}

// Test creates one node with testnet network ID and one node with main ID and verifies that connection fails
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

// Test creates a DAG on one node and verifies that the second node syncs with it and that the resulting DAG on the
// other end is the same
TEST_F(NetworkTest, node_sync) {
  auto node_cfgs = make_node_cfgs<5>(2);
  FullNode::Handle node1(node_cfgs[0], true);
  // Stop PBFT manager
  node1->getPbftManager()->stop();

  // Allow node to start up
  taraxa::thisThreadSleepForMilliSeconds(1000);

  std::vector<DagBlock> blks;
  // Generate DAG blocks
  auto dag_genesis = node1->getConfig().chain.dag_genesis_block.getHash();
  auto sk = node1->getSecretKey();
  auto vrf_sk = node1->getVrfSecretKey();
  vdf_sortition::VdfConfig vdf_config(node_cfgs[0].chain.vdf);

  auto propose_level = 1;
  vdf_sortition::VdfSortition vdf1(vdf_config, vrf_sk, getRlpBytes(propose_level));
  vdf1.computeVdfSolution(vdf_config, dag_genesis.asBytes());
  DagBlock blk1(dag_genesis, propose_level, {}, {}, vdf1, sk);

  propose_level = 2;
  vdf_sortition::VdfSortition vdf2(vdf_config, vrf_sk, getRlpBytes(propose_level));
  vdf2.computeVdfSolution(vdf_config, blk1.getHash().asBytes());
  DagBlock blk2(blk1.getHash(), propose_level, {}, {}, vdf2, sk);

  propose_level = 3;
  vdf_sortition::VdfSortition vdf3(vdf_config, vrf_sk, getRlpBytes(propose_level));
  vdf3.computeVdfSolution(vdf_config, blk2.getHash().asBytes());
  DagBlock blk3(blk2.getHash(), propose_level, {}, {}, vdf3, sk);

  propose_level = 4;
  vdf_sortition::VdfSortition vdf4(vdf_config, vrf_sk, getRlpBytes(propose_level));
  vdf4.computeVdfSolution(vdf_config, blk3.getHash().asBytes());
  DagBlock blk4(blk3.getHash(), propose_level, {}, {}, vdf4, sk);

  propose_level = 5;
  vdf_sortition::VdfSortition vdf5(vdf_config, vrf_sk, getRlpBytes(propose_level));
  vdf5.computeVdfSolution(vdf_config, blk4.getHash().asBytes());
  DagBlock blk5(blk4.getHash(), propose_level, {}, {}, vdf5, sk);

  propose_level = 6;
  vdf_sortition::VdfSortition vdf6(vdf_config, vrf_sk, getRlpBytes(propose_level));
  vdf6.computeVdfSolution(vdf_config, blk5.getHash().asBytes());
  DagBlock blk6(blk5.getHash(), propose_level, {blk4.getHash(), blk3.getHash()}, {}, vdf6, sk);

  blks.push_back(blk6);
  blks.push_back(blk5);
  blks.push_back(blk4);
  blks.push_back(blk3);
  blks.push_back(blk2);
  blks.push_back(blk1);

  for (size_t i = 0; i < blks.size(); ++i) {
    node1->getDagBlockManager()->insertBlock(blks[i]);
  }

  EXPECT_HAPPENS({30s, 500ms}, [&](auto& ctx) {
    WAIT_EXPECT_EQ(ctx, node1->getNumReceivedBlocks(), blks.size())
    WAIT_EXPECT_EQ(ctx, node1->getDagManager()->getNumVerticesInDag().first, 7)
    WAIT_EXPECT_EQ(ctx, node1->getDagManager()->getNumEdgesInDag().first, 8)
  });

  FullNode::Handle node2(node_cfgs[1], true);

  std::cout << "Waiting Sync..." << std::endl;
  EXPECT_HAPPENS({45s, 1500ms}, [&](auto& ctx) {
    WAIT_EXPECT_EQ(ctx, node2->getNumReceivedBlocks(), blks.size())
    WAIT_EXPECT_EQ(ctx, node2->getDagManager()->getNumVerticesInDag().first, 7)
    WAIT_EXPECT_EQ(ctx, node2->getDagManager()->getNumEdgesInDag().first, 8)
  });
}

// Test creates a PBFT chain on one node and verifies
// that the second node syncs with it and that the resulting
// chain on the other end is the same
TEST_F(NetworkTest, node_pbft_sync) {
  auto node_cfgs = make_node_cfgs<20>(2);
  FullNode::Handle node1(node_cfgs[0], true);

  // Stop PBFT manager and executor for syncing test
  node1->getPbftManager()->stop();

  auto db1 = node1->getDB();
  auto pbft_chain1 = node1->getPbftChain();

  auto dag_genesis = node1->getConfig().chain.dag_genesis_block.getHash();
  auto sk = node1->getSecretKey();
  auto vrf_sk = node1->getVrfSecretKey();
  vdf_sortition::VdfConfig vdf_config(node_cfgs[0].chain.vdf);
  auto batch = db1->createWriteBatch();

  // generate first PBFT block sample
  blk_hash_t prev_block_hash(0);
  uint64_t period = 1;
  addr_t beneficiary(987);

  level_t level = 1;
  vdf_sortition::VdfSortition vdf1(vdf_config, vrf_sk, getRlpBytes(level));
  vdf1.computeVdfSolution(vdf_config, dag_genesis.asBytes());
  DagBlock blk1(dag_genesis, 1, {}, {}, vdf1, sk);
  node1->getDagBlockManager()->insertBlock(blk1);

  PbftBlock pbft_block1(prev_block_hash, blk1.getHash(), period, beneficiary, node1->getSecretKey());
  db1->putFinalizedDagBlockHashesByAnchor(batch, pbft_block1.getPivotDagBlockHash(),
                                          {pbft_block1.getPivotDagBlockHash()});
  std::vector<Vote> votes_for_pbft_blk1;
  votes_for_pbft_blk1.emplace_back(
      node1->getPbftManager()->generateVote(pbft_block1.getBlockHash(), cert_vote_type, 1, 3, 0));
  std::cout << "Generate 1 vote for first PBFT block" << std::endl;
  // Add cert votes in DB
  db1->addCertVotesToBatch(pbft_block1.getBlockHash(), votes_for_pbft_blk1, batch);
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
  uint64_t expect_pbft_chain_size = 1;
  EXPECT_EQ(node1->getPbftChain()->getPbftChainSize(), expect_pbft_chain_size);

  // generate second PBFT block sample
  prev_block_hash = pbft_block1.getBlockHash();

  level = 2;
  vdf_sortition::VdfSortition vdf2(vdf_config, vrf_sk, getRlpBytes(level));
  vdf2.computeVdfSolution(vdf_config, blk1.getHash().asBytes());
  DagBlock blk2(blk1.getHash(), 2, {}, {}, vdf2, sk);
  node1->getDagBlockManager()->insertBlock(blk2);

  batch = db1->createWriteBatch();
  period = 2;
  beneficiary = addr_t(654);
  PbftBlock pbft_block2(prev_block_hash, blk2.getHash(), 2, beneficiary, node1->getSecretKey());
  db1->putFinalizedDagBlockHashesByAnchor(batch, pbft_block2.getPivotDagBlockHash(),
                                          {pbft_block2.getPivotDagBlockHash()});

  std::vector<Vote> votes_for_pbft_blk2;
  votes_for_pbft_blk2.emplace_back(
      node1->getPbftManager()->generateVote(pbft_block2.getBlockHash(), cert_vote_type, 2, 3, 0));
  std::cout << "Generate 1 vote for second PBFT block" << std::endl;
  // node1 put block2 into pbft chain and store into DB
  // Add cert votes in DB
  db1->addCertVotesToBatch(pbft_block2.getBlockHash(), votes_for_pbft_blk2, batch);
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
  auto node_cfgs = make_node_cfgs<20>(2);
  FullNode::Handle node1(node_cfgs[0], true);

  // Stop PBFT manager and executor for syncing test
  node1->getPbftManager()->stop();

  auto db1 = node1->getDB();
  auto pbft_chain1 = node1->getPbftChain();

  auto dag_genesis = node1->getConfig().chain.dag_genesis_block.getHash();
  auto sk = node1->getSecretKey();
  auto vrf_sk = node1->getVrfSecretKey();
  vdf_sortition::VdfConfig vdf_config(node_cfgs[0].chain.vdf);
  auto batch = db1->createWriteBatch();

  // generate first PBFT block sample
  blk_hash_t prev_block_hash(0);
  uint64_t period = 1;
  addr_t beneficiary(876);
  level_t level = 1;
  vdf_sortition::VdfSortition vdf1(vdf_config, vrf_sk, getRlpBytes(level));
  vdf1.computeVdfSolution(vdf_config, dag_genesis.asBytes());
  DagBlock blk1(dag_genesis, 1, {}, {}, vdf1, sk);
  node1->getDagBlockManager()->insertBlock(blk1);

  PbftBlock pbft_block1(prev_block_hash, blk1.getHash(), period, beneficiary, node1->getSecretKey());
  db1->putFinalizedDagBlockHashesByAnchor(batch, pbft_block1.getPivotDagBlockHash(),
                                          {pbft_block1.getPivotDagBlockHash()});
  std::vector<Vote> votes_for_pbft_blk1;
  votes_for_pbft_blk1.emplace_back(
      node1->getPbftManager()->generateVote(pbft_block1.getBlockHash(), cert_vote_type, 1, 3, 0));
  std::cout << "Generate 1 vote for first PBFT block" << std::endl;
  // Add cert votes in DB
  db1->addCertVotesToBatch(pbft_block1.getBlockHash(), votes_for_pbft_blk1, batch);
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
  level = 2;
  vdf_sortition::VdfSortition vdf2(vdf_config, vrf_sk, getRlpBytes(level));
  vdf2.computeVdfSolution(vdf_config, blk1.getHash().asBytes());
  DagBlock blk2(blk1.getHash(), 2, {}, {}, vdf2, sk);
  node1->getDagBlockManager()->insertBlock(blk2);

  batch = db1->createWriteBatch();
  period = 2;
  beneficiary = addr_t(654);

  PbftBlock pbft_block2(prev_block_hash, blk2.getHash(), period, beneficiary, node1->getSecretKey());
  db1->putFinalizedDagBlockHashesByAnchor(batch, pbft_block2.getPivotDagBlockHash(),
                                          {pbft_block2.getPivotDagBlockHash()});
  std::cout << "Use fake votes for the second PBFT block" << std::endl;
  // node1 put block2 into pbft chain and use fake votes storing into DB (malicious player)
  // Add fake votes in DB
  db1->addCertVotesToBatch(pbft_block2.getBlockHash(), votes_for_pbft_blk1, batch);
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
  uint64_t sync_pbft_chain_size = 1;
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

// Test PBFT next votes sycning when node is behind of PBFT round with peer
TEST_F(NetworkTest, pbft_next_votes_sync_in_behind_round) {
  auto node_cfgs = make_node_cfgs<20>(2);
  FullNode::Handle node1(node_cfgs[0], true);

  // Stop PBFT manager, that will place vote
  std::shared_ptr<PbftManager> pbft_mgr1 = node1->getPbftManager();
  pbft_mgr1->stop();

  // Generate 3 next votes
  std::vector<Vote> next_votes;
  uint64_t round = 1;
  size_t step = 5;
  size_t weighted_index;
  for (auto i = 0; i < 3; i++) {
    blk_hash_t voted_pbft_block_hash(i % 2);  // Next votes could vote on 2 values
    PbftVoteTypes type = next_vote_type;
    weighted_index = i;
    Vote vote = pbft_mgr1->generateVote(voted_pbft_block_hash, type, round, step, weighted_index);
    next_votes.emplace_back(vote);
  }

  // Update next votes bundle and set PBFT round
  auto pbft_2t_plus_1 = 1;
  node1->getNextVotesManager()->updateNextVotes(next_votes, pbft_2t_plus_1);
  pbft_mgr1->setPbftRound(2);  // Make sure node2 PBFT round is less than node1

  FullNode::Handle node2(node_cfgs[1], true);
  // Stop PBFT manager, that will place vote
  std::shared_ptr<PbftManager> pbft_mgr2 = node2->getPbftManager();
  pbft_mgr2->stop();

  std::shared_ptr<Network> nw1 = node1->getNetwork();
  std::shared_ptr<Network> nw2 = node2->getNetwork();
  // Wait node1 and node2 connect to each other
  unsigned node_peers = 1;
  for (int i = 0; i < 300; i++) {
    // test timeout is 30 seconds
    if (nw1->getPeerCount() == node_peers && nw2->getPeerCount() == node_peers) {
      break;
    }
    taraxa::thisThreadSleepForMilliSeconds(100);
  }
  EXPECT_EQ(nw1->getPeerCount(), 1);
  EXPECT_EQ(nw2->getPeerCount(), 1);

  node2->getVoteManager()->clearUnverifiedVotesTable();

  auto expect_size = next_votes.size();
  auto node2_vote_mgr = node2->getVoteManager();
  auto node2_next_votes_size = node2_vote_mgr->getUnverifiedVotesSize();
  // Wait 6 PBFT lambda time for sending network status
  auto sleep_time = node_cfgs[1].chain.pbft.lambda_ms_min * 6;
  for (auto i = 0; i < 10; i++) {
    if (node2_next_votes_size == expect_size) {
      break;
    }

    taraxa::thisThreadSleepForMilliSeconds(sleep_time);
    node2_next_votes_size = node2_vote_mgr->getUnverifiedVotesSize();
  }
  EXPECT_EQ(node2_next_votes_size, expect_size);
}

// Test PBFT next votes sycning when node has same PBFT round with peer, but has less previous round next votes size
TEST_F(NetworkTest, pbft_next_votes_sync_in_same_round_1) {
  auto pbft_previous_round = 0;
  auto pbft_2t_plus_1 = 2;

  auto node_cfgs = make_node_cfgs<20>(2);
  vector<FullNode::Handle> nodes(2);
  for (auto i(0); i < 2; i++) {
    nodes[i] = FullNode::Handle(node_cfgs[i], true);
    // Stop PBFT manager, that will place vote
    nodes[i]->getPbftManager()->stop();
  }
  EXPECT_TRUE(wait_connect(nodes) && wait_inital_status_packet_passed(nodes));

  auto& node1 = nodes[0];
  auto& node2 = nodes[1];

  // Generate 4 next votes for noode1
  std::vector<Vote> next_votes1;
  uint64_t round = 0;
  size_t step = 5;
  PbftVoteTypes type = next_vote_type;
  size_t weighted_index;
  for (auto i = 0; i < 4; i++) {
    blk_hash_t voted_pbft_block_hash1(i % 2);  // Next votes could vote on 2 values
    weighted_index = i;
    Vote vote = node1->getPbftManager()->generateVote(voted_pbft_block_hash1, type, round, step, weighted_index);
    next_votes1.emplace_back(vote);
  }

  auto node1_next_votes_mgr = node1->getNextVotesManager();
  // Update next votes bundle
  node1_next_votes_mgr->updateNextVotes(next_votes1, pbft_2t_plus_1);
  EXPECT_EQ(node1_next_votes_mgr->getNextVotesSize(), next_votes1.size());

  // Generate 2 same next votes with node1, voted same value on NULL_BLOCK_HASH
  blk_hash_t voted_pbft_block_hash2(0);
  std::vector<Vote> next_votes2;
  for (auto i = 0; i < 2; i++) {
    weighted_index = i * 2;  // NULL_BLOCK_HASH is weighted_index at 0 and 2
    Vote vote = node1->getPbftManager()->generateVote(voted_pbft_block_hash2, type, round, step, weighted_index);
    next_votes2.emplace_back(vote);
  }

  auto node2_next_votes_mgr = node2->getNextVotesManager();
  // Update next votes bundle
  node2_next_votes_mgr->updateNextVotes(next_votes2, pbft_2t_plus_1);
  EXPECT_EQ(node2_next_votes_mgr->getNextVotesSize(), next_votes2.size());

  // Set PBFT previous round 2t+1 for syncing
  node2->getDB()->savePbft2TPlus1(pbft_previous_round, pbft_2t_plus_1);

  auto expect_size = next_votes1.size();
  EXPECT_HAPPENS({30s, 500ms},
                 [&](auto& ctx) { WAIT_EXPECT_EQ(ctx, node2_next_votes_mgr->getNextVotesSize(), expect_size); });
}

// Test PBFT next votes sycning when node has same PBFT round with peer
TEST_F(NetworkTest, pbft_next_votes_sync_in_same_round_2) {
  auto pbft_previous_round = 0;
  auto pbft_2t_plus_1 = 3;

  auto node_cfgs = make_node_cfgs<20>(2);
  vector<FullNode::Handle> nodes(2);
  for (auto i(0); i < 2; i++) {
    nodes[i] = FullNode::Handle(node_cfgs[i], true);
    // Stop PBFT manager, that will place vote
    nodes[i]->getPbftManager()->stop();
  }
  EXPECT_TRUE(wait_connect(nodes) && wait_inital_status_packet_passed(nodes));

  auto& node1 = nodes[0];
  auto& node2 = nodes[1];

  // Generate 3 next votes for node1
  std::vector<Vote> next_votes1;
  blk_hash_t voted_pbft_block_hash1(blk_hash_t(0));
  PbftVoteTypes type = next_vote_type;
  uint64_t round = 0;
  size_t step = 5;
  size_t weighted_index;
  for (auto i = 0; i < 3; i++) {
    weighted_index = i;
    Vote vote = node1->getPbftManager()->generateVote(voted_pbft_block_hash1, type, round, step, weighted_index);
    next_votes1.emplace_back(vote);
  }

  auto next_votes_mgr1 = node1->getNextVotesManager();
  // Update node1 next votes bundle
  next_votes_mgr1->updateNextVotes(next_votes1, pbft_2t_plus_1);
  EXPECT_EQ(next_votes_mgr1->getNextVotesSize(), next_votes1.size());

  // Generate 3 different next votes with node1
  std::vector<Vote> next_votes2;
  step = 6;
  blk_hash_t voted_pbft_block_hash2(blk_hash_t(2));
  for (auto i = 0; i < 3; i++) {
    weighted_index = i;
    Vote vote = node2->getPbftManager()->generateVote(voted_pbft_block_hash2, type, round, step, weighted_index);
    next_votes2.emplace_back(vote);
  }

  auto next_votes_mgr2 = node2->getNextVotesManager();
  // Update node2 next votes bundle
  next_votes_mgr2->updateNextVotes(next_votes2, pbft_2t_plus_1);
  EXPECT_EQ(next_votes_mgr2->getNextVotesSize(), next_votes2.size());

  // Set node2 PBFT previous round 2t+1 for networking
  node2->getDB()->savePbft2TPlus1(pbft_previous_round, pbft_2t_plus_1);

  std::shared_ptr<Network> nw1 = node1->getNetwork();
  std::shared_ptr<Network> nw2 = node2->getNetwork();

  // Node1 broadcast next votes1 to node2
  nw1->broadcastPreviousRoundNextVotesBundle();

  auto node2_expect_size = next_votes1.size() + next_votes2.size();
  EXPECT_HAPPENS({5s, 100ms},
                 [&](auto& ctx) { WAIT_EXPECT_EQ(ctx, next_votes_mgr2->getNextVotesSize(), node2_expect_size) });

  // Expect node1 print out "ERROR: Cannot get PBFT 2t+1 in PBFT round 0"
  EXPECT_EQ(next_votes_mgr1->getNextVotesSize(), next_votes1.size());

  // Set node1 PBFT previous round 2t+1 for networking
  node1->getDB()->savePbft2TPlus1(pbft_previous_round, pbft_2t_plus_1);

  // Node2 broadcast updated next votes to node1
  nw2->broadcastPreviousRoundNextVotesBundle();

  auto node1_expect_size = next_votes1.size() + next_votes2.size();
  EXPECT_HAPPENS({5s, 100ms},
                 [&](auto& ctx) { WAIT_EXPECT_EQ(ctx, next_votes_mgr1->getNextVotesSize(), node1_expect_size) });
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
  vdf_sortition::VdfConfig vdf_config(node_cfgs[0].chain.vdf);
  auto propose_level = 1;
  vdf_sortition::VdfSortition vdf1(vdf_config, vrf_sk, getRlpBytes(propose_level));
  vdf1.computeVdfSolution(vdf_config, dag_genesis.asBytes());
  DagBlock blk1(dag_genesis, propose_level, {}, {g_signed_trx_samples[0].getHash(), g_signed_trx_samples[1].getHash()},
                vdf1, sk);
  std::vector<Transaction> tr1({g_signed_trx_samples[0], g_signed_trx_samples[1]});

  propose_level = 2;
  vdf_sortition::VdfSortition vdf2(vdf_config, vrf_sk, getRlpBytes(propose_level));
  vdf2.computeVdfSolution(vdf_config, blk1.getHash().asBytes());
  DagBlock blk2(blk1.getHash(), propose_level, {}, {g_signed_trx_samples[2].getHash()}, vdf2, sk);
  std::vector<Transaction> tr2({g_signed_trx_samples[2]});

  propose_level = 3;
  vdf_sortition::VdfSortition vdf3(vdf_config, vrf_sk, getRlpBytes(propose_level));
  vdf3.computeVdfSolution(vdf_config, blk2.getHash().asBytes());
  DagBlock blk3(blk2.getHash(), propose_level, {}, {}, vdf3, sk);
  std::vector<Transaction> tr3;

  propose_level = 4;
  vdf_sortition::VdfSortition vdf4(vdf_config, vrf_sk, getRlpBytes(propose_level));
  vdf4.computeVdfSolution(vdf_config, blk3.getHash().asBytes());
  DagBlock blk4(blk3.getHash(), propose_level, {},
                {g_signed_trx_samples[3].getHash(), g_signed_trx_samples[4].getHash()}, vdf4, sk);
  std::vector<Transaction> tr4({g_signed_trx_samples[3], g_signed_trx_samples[4]});

  propose_level = 5;
  vdf_sortition::VdfSortition vdf5(vdf_config, vrf_sk, getRlpBytes(propose_level));
  vdf5.computeVdfSolution(vdf_config, blk4.getHash().asBytes());
  DagBlock blk5(blk4.getHash(), propose_level, {},
                {g_signed_trx_samples[5].getHash(), g_signed_trx_samples[6].getHash(),
                 g_signed_trx_samples[7].getHash(), g_signed_trx_samples[8].getHash()},
                vdf5, sk);
  std::vector<Transaction> tr5(
      {g_signed_trx_samples[5], g_signed_trx_samples[6], g_signed_trx_samples[7], g_signed_trx_samples[8]});

  propose_level = 6;
  vdf_sortition::VdfSortition vdf6(vdf_config, vrf_sk, getRlpBytes(propose_level));
  vdf6.computeVdfSolution(vdf_config, blk5.getHash().asBytes());
  DagBlock blk6(blk5.getHash(), propose_level, {blk4.getHash(), blk3.getHash()}, {g_signed_trx_samples[9].getHash()},
                vdf6, sk);
  std::vector<Transaction> tr6({g_signed_trx_samples[9]});

  node1->getDagBlockManager()->insertBroadcastedBlockWithTransactions(blk6, tr6);
  node1->getDagBlockManager()->insertBroadcastedBlockWithTransactions(blk5, tr5);
  node1->getDagBlockManager()->insertBroadcastedBlockWithTransactions(blk4, tr4);
  node1->getDagBlockManager()->insertBroadcastedBlockWithTransactions(blk3, tr3);
  node1->getDagBlockManager()->insertBroadcastedBlockWithTransactions(blk2, tr2);
  node1->getDagBlockManager()->insertBroadcastedBlockWithTransactions(blk1, tr1);

  // To make sure blocks are stored before starting node 2
  taraxa::thisThreadSleepForMilliSeconds(1000);

  FullNode::Handle node2(node_cfgs[1], true);
  std::cout << "Waiting Sync for up to 20000 milliseconds ..." << std::endl;
  for (int i = 0; i < 40; i++) {
    taraxa::thisThreadSleepForMilliSeconds(1000);
    if (node2->getDagManager()->getNumVerticesInDag().first == 7 &&
        node2->getDagManager()->getNumEdgesInDag().first == 8)
      break;
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
  vdf_sortition::VdfConfig vdf_config(node_cfgs[0].chain.vdf);
  auto transactions = samples::createSignedTrxSamples(0, NUM_TRX2, sk);
  // DAG block1
  auto propose_level = 1;
  vdf_sortition::VdfSortition vdf1(vdf_config, vrf_sk, getRlpBytes(propose_level));
  vdf1.computeVdfSolution(vdf_config, dag_genesis.asBytes());
  DagBlock blk1(dag_genesis, propose_level, {}, {transactions[0].getHash(), transactions[1].getHash()}, vdf1, sk);
  std::vector<Transaction> tr1({transactions[0], transactions[1]});
  // DAG block2
  propose_level = 1;
  vdf_sortition::VdfSortition vdf2(vdf_config, vrf_sk, getRlpBytes(propose_level));
  vdf2.computeVdfSolution(vdf_config, dag_genesis.asBytes());
  DagBlock blk2(dag_genesis, propose_level, {}, {transactions[2].getHash(), transactions[3].getHash()}, vdf2, sk);
  std::vector<Transaction> tr2({transactions[2], transactions[3]});
  // DAG block3
  propose_level = 2;
  vdf_sortition::VdfSortition vdf3(vdf_config, vrf_sk, getRlpBytes(propose_level));
  vdf3.computeVdfSolution(vdf_config, blk1.getHash().asBytes());
  DagBlock blk3(blk1.getHash(), propose_level, {}, {transactions[4].getHash(), transactions[5].getHash()}, vdf3, sk);
  std::vector<Transaction> tr3({transactions[4], transactions[5]});
  // DAG block4
  propose_level = 3;
  vdf_sortition::VdfSortition vdf4(vdf_config, vrf_sk, getRlpBytes(propose_level));
  vdf4.computeVdfSolution(vdf_config, blk3.getHash().asBytes());
  DagBlock blk4(blk3.getHash(), propose_level, {}, {transactions[6].getHash(), transactions[7].getHash()}, vdf4, sk);
  std::vector<Transaction> tr4({transactions[6], transactions[7]});
  // DAG block5
  propose_level = 2;
  vdf_sortition::VdfSortition vdf5(vdf_config, vrf_sk, getRlpBytes(propose_level));
  vdf5.computeVdfSolution(vdf_config, blk2.getHash().asBytes());
  DagBlock blk5(blk2.getHash(), propose_level, {}, {transactions[8].getHash(), transactions[9].getHash()}, vdf5, sk);
  std::vector<Transaction> tr5({transactions[8], transactions[9]});
  // DAG block6
  propose_level = 2;
  vdf_sortition::VdfSortition vdf6(vdf_config, vrf_sk, getRlpBytes(propose_level));
  vdf6.computeVdfSolution(vdf_config, blk1.getHash().asBytes());
  DagBlock blk6(blk1.getHash(), propose_level, {}, {transactions[10].getHash(), transactions[11].getHash()}, vdf6, sk);
  std::vector<Transaction> tr6({transactions[10], transactions[11]});
  // DAG block7
  propose_level = 3;
  vdf_sortition::VdfSortition vdf7(vdf_config, vrf_sk, getRlpBytes(propose_level));
  vdf7.computeVdfSolution(vdf_config, blk6.getHash().asBytes());
  DagBlock blk7(blk6.getHash(), propose_level, {}, {transactions[12].getHash(), transactions[13].getHash()}, vdf7, sk);
  std::vector<Transaction> tr7({transactions[12], transactions[13]});
  // DAG block8
  propose_level = 4;
  vdf_sortition::VdfSortition vdf8(vdf_config, vrf_sk, getRlpBytes(propose_level));
  vdf8.computeVdfSolution(vdf_config, blk1.getHash().asBytes());
  DagBlock blk8(blk1.getHash(), propose_level, {blk7.getHash()},
                {transactions[14].getHash(), transactions[15].getHash()}, vdf8, sk);
  std::vector<Transaction> tr8({transactions[14], transactions[15]});
  // DAG block9
  propose_level = 2;
  vdf_sortition::VdfSortition vdf9(vdf_config, vrf_sk, getRlpBytes(propose_level));
  vdf9.computeVdfSolution(vdf_config, blk1.getHash().asBytes());
  DagBlock blk9(blk1.getHash(), propose_level, {}, {transactions[16].getHash(), transactions[17].getHash()}, vdf9, sk);
  std::vector<Transaction> tr9({transactions[16], transactions[17]});
  // DAG block10
  propose_level = 5;
  vdf_sortition::VdfSortition vdf10(vdf_config, vrf_sk, getRlpBytes(propose_level));
  vdf10.computeVdfSolution(vdf_config, blk8.getHash().asBytes());
  DagBlock blk10(blk8.getHash(), propose_level, {}, {transactions[18].getHash(), transactions[19].getHash()}, vdf10,
                 sk);
  std::vector<Transaction> tr10({transactions[18], transactions[19]});
  // DAG block11
  propose_level = 3;
  vdf_sortition::VdfSortition vdf11(vdf_config, vrf_sk, getRlpBytes(propose_level));
  vdf11.computeVdfSolution(vdf_config, blk3.getHash().asBytes());
  DagBlock blk11(blk3.getHash(), propose_level, {}, {transactions[20].getHash(), transactions[21].getHash()}, vdf11,
                 sk);
  std::vector<Transaction> tr11({transactions[20], transactions[21]});
  // DAG block12
  propose_level = 3;
  vdf_sortition::VdfSortition vdf12(vdf_config, vrf_sk, getRlpBytes(propose_level));
  vdf12.computeVdfSolution(vdf_config, blk5.getHash().asBytes());
  DagBlock blk12(blk5.getHash(), propose_level, {}, {transactions[22].getHash(), transactions[23].getHash()}, vdf12,
                 sk);
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

  for (size_t i = 0; i < blks.size(); ++i) {
    node1->getDagBlockManager()->insertBroadcastedBlockWithTransactions(blks[i], trxs[i]);
  }

  taraxa::thisThreadSleepForMilliSeconds(200);

  FullNode::Handle node2(node_cfgs[1], true);
  std::cout << "Waiting Sync for up to 40000 milliseconds ..." << std::endl;
  for (int i = 0; i < 400; i++) {
    taraxa::thisThreadSleepForMilliSeconds(100);
    if (node2->getDagManager()->getNumVerticesInDag().first == 13 &&
        node2->getDagManager()->getNumEdgesInDag().first == 13) {
      break;
    }
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
  auto node_cfgs = make_node_cfgs<20>(numberOfNodes);
  auto nodes = launch_nodes(slice(node_cfgs, 0, numberOfNodes - 1));

  std::random_device dev;
  std::mt19937 rng(dev());
  std::uniform_int_distribution<std::mt19937::result_type> distTransactions(1, 20);
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

  wait({60s, 500ms}, [&](auto& ctx) {
    // Check 4 nodes DAG syncing
    for (int j = 1; j < numberOfNodes - 1; j++) {
      if (ctx.fail_if(nodes[j]->getDagManager()->getNumVerticesInDag().first !=
                      nodes[0]->getDagManager()->getNumVerticesInDag().first)) {
        return;
      }
    }
  });

  bool dag_synced = true;
  auto node0_vertices = nodes[0]->getDagManager()->getNumVerticesInDag().first;
  cout << "node0 vertices " << node0_vertices << endl;
  for (int i(1); i < numberOfNodes - 1; i++) {
    auto node_vertices = nodes[i]->getDagManager()->getNumVerticesInDag().first;
    cout << "node" << i << " vertices " << node_vertices << endl;
    if (node_vertices != node0_vertices) {
      dag_synced = false;
    }
  }
  // When last level have more than 1 DAG blocks, send a dummy transaction to converge DAG
  if (!dag_synced) {
    cout << "Send dummy trx" << endl;
    Transaction dummy_trx(counter++, 0, 2, TEST_TX_GAS_LIMIT, bytes(), nodes[0]->getSecretKey(),
                          nodes[0]->getAddress());
    // broadcast dummy transaction
    nodes[0]->getTransactionManager()->insertTransaction(dummy_trx, false);

    wait({60s, 500ms}, [&](auto& ctx) {
      // Check 4 nodes DAG syncing
      for (int j = 1; j < numberOfNodes - 1; j++) {
        WAIT_EXPECT_EQ(ctx, nodes[j]->getDagManager()->getNumVerticesInDag().first,
                       nodes[0]->getDagManager()->getNumVerticesInDag().first);
        ctx.fail_if(nodes[j]->getNetwork()->pbft_syncing());
      }
    });
  }

  EXPECT_GT(nodes[0]->getDagManager()->getNumVerticesInDag().first, 0);
  for (int i(1); i < numberOfNodes - 1; i++) {
    std::cout << "Index i " << i << std::endl;
    EXPECT_GT(nodes[i]->getDagManager()->getNumVerticesInDag().first, 0);
    EXPECT_EQ(nodes[i]->getDagManager()->getNumVerticesInDag().first,
              nodes[0]->getDagManager()->getNumVerticesInDag().first);
    EXPECT_EQ(nodes[i]->getDagManager()->getNumVerticesInDag().first, nodes[i]->getDB()->getNumDagBlocks());
    EXPECT_EQ(nodes[i]->getDagManager()->getNumEdgesInDag().first, nodes[0]->getDagManager()->getNumEdgesInDag().first);
    EXPECT_TRUE(!nodes[i]->getNetwork()->pbft_syncing());
  }

  // Bootstrapping node5 join the network
  nodes.emplace_back(FullNode::Handle(node_cfgs[numberOfNodes - 1], true));
  EXPECT_TRUE(wait_connect(nodes));

  std::cout << "Waiting Sync for node5..." << std::endl;
  wait({60s, 500ms}, [&](auto& ctx) {
    // Check 5 nodes DAG syncing
    for (int j = 1; j < numberOfNodes; j++) {
      if (ctx.fail_if(nodes[j]->getDagManager()->getNumVerticesInDag().first !=
                      nodes[0]->getDagManager()->getNumVerticesInDag().first)) {
        return;
      }
    }
  });

  dag_synced = true;
  node0_vertices = nodes[0]->getDagManager()->getNumVerticesInDag().first;
  cout << "node0 vertices " << node0_vertices << endl;
  for (int i(1); i < numberOfNodes; i++) {
    auto node_vertices = nodes[i]->getDagManager()->getNumVerticesInDag().first;
    cout << "node" << i << " vertices " << node_vertices << endl;
    if (node_vertices != node0_vertices) {
      dag_synced = false;
    }
  }
  // When last level have more than 1 DAG blocks, send a dummy transaction to converge DAG
  if (!dag_synced) {
    cout << "Send dummy trx" << endl;
    Transaction dummy_trx(counter++, 0, 2, TEST_TX_GAS_LIMIT, bytes(), nodes[0]->getSecretKey(),
                          nodes[0]->getAddress());
    // broadcast dummy transaction
    nodes[0]->getTransactionManager()->insertTransaction(dummy_trx, false);

    wait({60s, 500ms}, [&](auto& ctx) {
      // Check all 5 nodes DAG syncing
      for (int j = 1; j < numberOfNodes; j++) {
        WAIT_EXPECT_EQ(ctx, nodes[j]->getDagManager()->getNumVerticesInDag().first,
                       nodes[0]->getDagManager()->getNumVerticesInDag().first);
        ctx.fail_if(nodes[j]->getNetwork()->pbft_syncing());
      }
    });
  }

  EXPECT_GT(nodes[0]->getDagManager()->getNumVerticesInDag().first, 0);
  for (int i = 0; i < numberOfNodes; i++) {
    std::cout << "Index i " << i << std::endl;
    EXPECT_GT(nodes[i]->getDagManager()->getNumVerticesInDag().first, 0);
    EXPECT_EQ(nodes[i]->getDagManager()->getNumVerticesInDag().first,
              nodes[0]->getDagManager()->getNumVerticesInDag().first);
    EXPECT_EQ(nodes[i]->getDagManager()->getNumVerticesInDag().first, nodes[i]->getDB()->getNumDagBlocks());
    EXPECT_EQ(nodes[i]->getDagManager()->getNumEdgesInDag().first, nodes[0]->getDagManager()->getNumEdgesInDag().first);
    EXPECT_TRUE(!nodes[i]->getNetwork()->pbft_syncing());
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
  auto logging = logger::createDefaultLoggingConfig();
  logging.verbosity = logger::Verbosity::Error;

  addr_t node_addr;
  logger::InitLogging(logging, node_addr);

  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}