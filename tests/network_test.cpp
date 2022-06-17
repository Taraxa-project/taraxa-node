#include "network/network.hpp"

#include <gtest/gtest.h>

#include <atomic>
#include <iostream>
#include <vector>

#include "common/lazy.hpp"
#include "common/static_init.hpp"
#include "dag/block_proposer.hpp"
#include "dag/dag.hpp"
#include "logger/logger.hpp"
#include "network/tarcap/packets_handlers/dag_block_packet_handler.hpp"
#include "network/tarcap/packets_handlers/get_dag_sync_packet_handler.hpp"
#include "network/tarcap/packets_handlers/pbft_block_packet_handler.hpp"
#include "network/tarcap/packets_handlers/transaction_packet_handler.hpp"
#include "network/tarcap/packets_handlers/votes_sync_packet_handler.hpp"
#include "pbft/pbft_manager.hpp"
#include "util_test/samples.hpp"
#include "util_test/util.hpp"

namespace taraxa::core_tests {

using dev::p2p::Host;
using vrf_wrapper::VrfSortitionBase;

const unsigned NUM_TRX = 10;
auto g_secret = Lazy([] {
  return dev::Secret("3800b2875669d9b2053c1aff9224ecfdc411423aac5b5a73d7a45ced1c3b9dcd",
                     dev::Secret::ConstructFromStringType::FromHex);
});
auto node_key = dev::KeyPair(g_secret);
auto g_signed_trx_samples = Lazy([] { return samples::createSignedTrxSamples(0, NUM_TRX, g_secret); });

auto three_default_configs = Lazy([] { return make_node_cfgs(3); });
auto g_conf1 = Lazy([] { return three_default_configs[0]; });
auto g_conf2 = Lazy([] { return three_default_configs[1]; });
auto g_conf3 = Lazy([] { return three_default_configs[2]; });

struct NetworkTest : BaseTest {};

// Test creates two Network setup and verifies sending block between is successfull
TEST_F(NetworkTest, transfer_block) {
  auto nw1 = std::make_unique<Network>(g_conf1->network);
  auto nw2 = std::make_unique<Network>(g_conf2->network);

  nw1->start();
  nw2->start();
  DagBlock blk(blk_hash_t(1111), 0, {blk_hash_t(222), blk_hash_t(333), blk_hash_t(444)},
               {g_signed_trx_samples[0]->getHash(), g_signed_trx_samples[1]->getHash()}, sig_t(7777), blk_hash_t(888),
               addr_t(999));

  std::vector<std::pair<std::shared_ptr<Transaction>, TransactionStatus>> transactions{
      {g_signed_trx_samples[0], TransactionStatus::Verified}, {g_signed_trx_samples[1], TransactionStatus::Verified}};
  nw2->getSpecificHandler<network::tarcap::TransactionPacketHandler>()->onNewTransactions(std::move(transactions));

  EXPECT_HAPPENS({10s, 200ms}, [&](auto& ctx) {
    nw1->setPendingPeersToReady();
    nw2->setPendingPeersToReady();
    WAIT_EXPECT_EQ(ctx, nw1->getPeerCount(), 1)
    WAIT_EXPECT_EQ(ctx, nw2->getPeerCount(), 1)
  });

  nw2->getSpecificHandler<network::tarcap::DagBlockPacketHandler>()->sendBlock(nw1->getNodeId(), blk, {});

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

// Test creates two Network setup and verifies sending blocks between is successfull
// This test can not work anymore as we are marking other nodes as malicous becasue of invalid dag blocks
TEST_F(NetworkTest, transfer_lot_of_blocks) {
  auto node_cfgs = make_node_cfgs<20>(2);
  auto nodes = launch_nodes(node_cfgs);
  const auto& node1 = nodes[0];
  const auto& node2 = nodes[1];

  // Stop PBFT manager
  node1->getPbftManager()->stop();
  node2->getPbftManager()->stop();

  const auto db1 = node1->getDB();
  const auto dag_blk_mgr1 = node1->getDagBlockManager();
  const auto dag_blk_mgr2 = node2->getDagBlockManager();
  const auto nw1 = node1->getNetwork();
  const auto nw2 = node2->getNetwork();

  const auto trxs = samples::createSignedTrxSamples(0, 1500, g_secret);
  const auto estimation = node1->getTransactionManager()->estimateTransactionGas(trxs[0], {});
  const std::vector<uint64_t> estimations(trxs.size(), estimation);

  // node1 add one valid block
  const auto proposal_level = 1;
  const auto proposal_period = *db1->getProposalPeriodForDagLevel(proposal_level);
  const auto period_block_hash = db1->getPeriodBlockHash(proposal_period);
  const auto sortition_params = dag_blk_mgr1->sortitionParamsManager().getSortitionParams(proposal_period);
  vdf_sortition::VdfSortition vdf(sortition_params, node1->getVrfSecretKey(),
                                  VrfSortitionBase::makeVrfInput(proposal_level, period_block_hash));
  const auto dag_genesis = node1->getConfig().chain.dag_genesis_block.getHash();
  vdf.computeVdfSolution(sortition_params, dag_genesis.asBytes(), false);
  DagBlock blk(dag_genesis, proposal_level, {}, {trxs[0]->getHash()}, {estimation}, vdf, node1->getSecretKey());
  auto block_hash = blk.getHash();
  std::vector<std::shared_ptr<DagBlock>> dag_blocks;
  dag_blocks.emplace_back(std::make_shared<DagBlock>(std::move(blk)));

  // creating lot of blocks just for size
  std::vector<trx_hash_t> trx_hashes;
  std::vector<std::pair<std::shared_ptr<Transaction>, TransactionStatus>> verified_transactions;
  trx_hashes.reserve(trxs.size());
  verified_transactions.reserve(trxs.size());

  for (const auto& trx : trxs) {
    trx_hashes.push_back(trx->getHash());
    verified_transactions.push_back({trx, TransactionStatus::Verified});
  }

  for (int i = 0; i < 100; ++i) {
    const auto proposal_period = *db1->getProposalPeriodForDagLevel(proposal_level + 1);
    const auto period_block_hash = db1->getPeriodBlockHash(proposal_period);
    const auto sortition_params = dag_blk_mgr1->sortitionParamsManager().getSortitionParams(proposal_period);
    vdf_sortition::VdfSortition vdf(sortition_params, node1->getVrfSecretKey(),
                                    VrfSortitionBase::makeVrfInput(proposal_level + 1, period_block_hash));
    DagBlock blk(block_hash, proposal_level + 1, {}, trx_hashes, {}, vdf, node1->getSecretKey());
    dag_blocks.emplace_back(std::make_shared<DagBlock>(blk));
  }

  nw1->getSpecificHandler<network::tarcap::TransactionPacketHandler>()->onNewTransactions(
      std::move(verified_transactions));
  for (auto block : dag_blocks) {
    dag_blk_mgr1->insertAndVerifyBlock(DagBlock(*block));
  }
  wait({1s, 200ms}, [&](auto& ctx) { WAIT_EXPECT_NE(ctx, dag_blk_mgr1->getDagBlock(block_hash), nullptr) });

  taraxa::thisThreadSleepForSeconds(1);
  const auto node1_period = node1->getPbftChain()->getPbftChainSize();
  const auto node2_period = node2->getPbftChain()->getPbftChainSize();
  std::cout << "node1 period " << node1_period << ", node2 period " << node2_period << std::endl;
  nw1->getSpecificHandler<network::tarcap::GetDagSyncPacketHandler>()->sendBlocks(
      nw2->getNodeId(), std::move(dag_blocks), {}, node2_period, node1_period);

  std::cout << "Waiting Sync ..." << std::endl;
  wait({30s, 200ms}, [&](auto& ctx) { WAIT_EXPECT_NE(ctx, dag_blk_mgr2->getDagBlock(block_hash), nullptr) });
}

TEST_F(NetworkTest, send_pbft_block) {
  auto node_cfgs = make_node_cfgs<5>(2);
  auto nodes = launch_nodes(node_cfgs);
  auto nw1 = nodes[0]->getNetwork();
  auto nw2 = nodes[1]->getNetwork();

  auto pbft_block = make_simple_pbft_block(blk_hash_t(1), 2, node_cfgs[0].chain.dag_genesis_block.getHash());
  uint64_t chain_size = 111;

  nw2->getSpecificHandler<network::tarcap::PbftBlockPacketHandler>()->sendPbftBlock(nw1->getNodeId(), pbft_block,
                                                                                    chain_size);

  auto node2_id = nw2->getNodeId();
  EXPECT_HAPPENS({10s, 200ms},
                 [&](auto& ctx) { WAIT_EXPECT_EQ(ctx, nw1->getPeer(node2_id)->pbft_chain_size_, chain_size) });
}

TEST_F(NetworkTest, malicious_peers) {
  NetworkConfig conf;
  conf.network_peer_blacklist_timeout = 2;
  std::shared_ptr<dev::p2p::Host> host;
  EXPECT_EQ(conf.disable_peer_blacklist, false);
  network::tarcap::PeersState state1(host, dev::p2p::NodeID(), conf);
  dev::p2p::NodeID id1(1);
  dev::p2p::NodeID id2(2);
  state1.set_peer_malicious(id1);
  EXPECT_EQ(state1.is_peer_malicious(id1), true);
  EXPECT_EQ(state1.is_peer_malicious(id2), false);

  conf.network_peer_blacklist_timeout = 0;
  network::tarcap::PeersState state2(host, dev::p2p::NodeID(), conf);
  state2.set_peer_malicious(id1);
  EXPECT_EQ(state2.is_peer_malicious(id1), true);
  EXPECT_EQ(state2.is_peer_malicious(id2), false);

  conf.network_peer_blacklist_timeout = 2;
  conf.disable_peer_blacklist = true;
  network::tarcap::PeersState state3(host, dev::p2p::NodeID(), conf);
  state1.set_peer_malicious(id1);
  EXPECT_EQ(state3.is_peer_malicious(id1), false);
  EXPECT_EQ(state3.is_peer_malicious(id2), false);

  conf.network_peer_blacklist_timeout = 0;
  conf.disable_peer_blacklist = true;
  network::tarcap::PeersState state4(host, dev::p2p::NodeID(), conf);
  state1.set_peer_malicious(id1);
  EXPECT_EQ(state4.is_peer_malicious(id1), false);
  EXPECT_EQ(state4.is_peer_malicious(id2), false);

  thisThreadSleepForMilliSeconds(3100);

  EXPECT_EQ(state1.is_peer_malicious(id1), false);
  EXPECT_EQ(state1.is_peer_malicious(id2), false);

  EXPECT_EQ(state2.is_peer_malicious(id1), true);
  EXPECT_EQ(state2.is_peer_malicious(id2), false);

  EXPECT_EQ(state3.is_peer_malicious(id1), false);
  EXPECT_EQ(state3.is_peer_malicious(id2), false);

  EXPECT_EQ(state4.is_peer_malicious(id1), false);
  EXPECT_EQ(state4.is_peer_malicious(id2), false);
}

TEST_F(NetworkTest, sync_large_pbft_block) {
  const uint32_t MAX_PACKET_SIZE = 15 * 1024 * 1024;  // 15 MB -> 15 * 1024 * 1024 B
  auto node_cfgs = make_node_cfgs<5>(2);
  node_cfgs[0].chain.pbft.gas_limit = FinalChain::GAS_LIMIT;
  node_cfgs[1].chain.pbft.gas_limit = FinalChain::GAS_LIMIT;
  auto nodes = launch_nodes({node_cfgs[0]});

  // Create 250 transactions, each one has 10k dummy data
  bytes dummy_100k_data(100000, 0);
  auto signed_trxs = samples::createSignedTrxSamples(0, 500, nodes[0]->getSecretKey(), dummy_100k_data);

  // node1 own all coins, could produce blocks by itself
  nodes[0]->getPbftManager()->stop();

  auto nw1 = nodes[0]->getNetwork();

  for (size_t i = 0; i < signed_trxs.size(); i++) {
    // Splits transactions into multiple dag blocks. Size of dag blocks should be about 5MB for 50 10k transactions
    if ((i + 1) % 50 == 0) {
      wait({20s, 10ms}, [&](auto& ctx) {
        auto trx_pool_size = nodes[0]->getTransactionManager()->getTransactionPoolSize();
        ctx.fail_if(trx_pool_size > 0);
      });
    }
    nodes[0]->getTransactionManager()->insertTransaction(signed_trxs[i]);
  }

  const auto node1_pbft_chain = nodes[0]->getPbftChain();
  nodes[0]->getPbftManager()->start();
  EXPECT_HAPPENS({30s, 100ms}, [&](auto& ctx) {
    WAIT_EXPECT_GT(ctx, node1_pbft_chain->getPbftChainSizeExcludingEmptyPbftBlocks(), 0)
  });
  nodes[0]->getPbftManager()->stop();

  // Verify that a block over MAX_PACKET_SIZE is created
  auto total_size = 0;
  auto non_empty_last_period = node1_pbft_chain->getPbftChainSize();
  while (non_empty_last_period > 0) {
    auto pbft_block = nodes[0]->getDB()->getPbftBlock(non_empty_last_period);
    if (!pbft_block.has_value()) {
      non_empty_last_period--;
      continue;
    }
    total_size = pbft_block->rlp(true).size();
    auto blocks = nodes[0]->getDB()->getFinalizedDagBlockHashesByPeriod(non_empty_last_period);
    for (auto b : blocks) {
      auto block = nodes[0]->getDB()->getDagBlock(b);
      EXPECT_NE(block, nullptr);
      total_size += block->rlp(true).size();
      for (auto t : block->getTrxs()) {
        auto trx = nodes[0]->getDB()->getTransaction(t);
        EXPECT_NE(trx, nullptr);
        total_size += trx->rlp().size();
      }
    }
    break;
  }
  EXPECT_GT(total_size, MAX_PACKET_SIZE);

  // Launch node2, node2 own 0 balance, could not vote
  auto nodes2 = launch_nodes({node_cfgs[1]});
  nodes[0]->getPbftManager()->stop();
  const auto node2_pbft_chain = nodes2[0]->getPbftChain();

  // verify that the large pbft block is synced
  EXPECT_HAPPENS({30s, 100ms}, [&](auto& ctx) {
    WAIT_EXPECT_EQ(ctx, node2_pbft_chain->getPbftChainSize(), node1_pbft_chain->getPbftChainSize())
  });

  auto pbft_blocks1 = nodes[0]->getDB()->getPbftBlock(non_empty_last_period);
  auto pbft_blocks2 = nodes2[0]->getDB()->getPbftBlock(non_empty_last_period);
  if (pbft_blocks1->rlp(true) != pbft_blocks2->rlp(true)) {
    std::cout << "PBFT block1 " << *pbft_blocks1 << std::endl;
    std::cout << "PBFT block2 " << *pbft_blocks2 << std::endl;
  }
  EXPECT_EQ(pbft_blocks1->rlp(true), pbft_blocks2->rlp(true));

  // this sleep is needed to process all remaining packets and destruct all network stuff
  // on removal will cause next tests in the suite to fail because p2p port left binded
  // see https://github.com/Taraxa-project/taraxa-node/issues/977 for more info
  std::this_thread::sleep_for(1s);
}

// Test creates two Network setup and verifies sending transaction
// between is successfull
TEST_F(NetworkTest, transfer_transaction) {
  auto nw1 = std::make_unique<Network>(g_conf1->network);
  auto nw2 = std::make_unique<Network>(g_conf2->network);
  nw1->start();
  nw2->start();

  EXPECT_HAPPENS({10s, 200ms}, [&](auto& ctx) {
    nw1->setPendingPeersToReady();
    nw2->setPendingPeersToReady();
    WAIT_EXPECT_EQ(ctx, nw1->getPeerCount(), 1)
    WAIT_EXPECT_EQ(ctx, nw2->getPeerCount(), 1)
  });

  auto nw1_nodeid = nw1->getNodeId();
  auto nw2_nodeid = nw2->getNodeId();
  EXPECT_NE(nw1->getPeer(nw2_nodeid), nullptr);
  EXPECT_NE(nw2->getPeer(nw1_nodeid), nullptr);

  std::vector<taraxa::bytes> transactions;
  transactions.push_back(g_signed_trx_samples[0]->rlp());
  transactions.push_back(g_signed_trx_samples[1]->rlp());
  transactions.push_back(g_signed_trx_samples[2]->rlp());

  nw2->getSpecificHandler<network::tarcap::TransactionPacketHandler>()->sendTransactions(nw1_nodeid, transactions);

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
  h256 genesis_hash;
  {
    std::shared_ptr<Network> nw1 =
        std::make_shared<taraxa::Network>(g_conf1->network, genesis_hash, Host::CapabilitiesFactory());
    std::shared_ptr<Network> nw2 = std::make_shared<taraxa::Network>(g_conf2->network, genesis_hash,
                                                                     Host::CapabilitiesFactory(), "/tmp/nw2", key2);
    std::shared_ptr<Network> nw3 = std::make_shared<taraxa::Network>(g_conf3->network, genesis_hash,
                                                                     Host::CapabilitiesFactory(), "/tmp/nw3", key3);

    nw1->start();
    nw2->start();
    nw3->start();

    EXPECT_HAPPENS({120s, 500ms}, [&](auto& ctx) {
      nw1->setPendingPeersToReady();
      nw2->setPendingPeersToReady();
      nw3->setPendingPeersToReady();
      WAIT_EXPECT_EQ(ctx, nw1->getPeerCount(), 2)
      WAIT_EXPECT_EQ(ctx, nw2->getPeerCount(), 2)
      WAIT_EXPECT_EQ(ctx, nw3->getPeerCount(), 2)
    });
  }

  std::shared_ptr<Network> nw2 =
      std::make_shared<taraxa::Network>(g_conf2->network, genesis_hash, Host::CapabilitiesFactory(), "/tmp/nw2", key2);
  std::shared_ptr<Network> nw3 =
      std::make_shared<taraxa::Network>(g_conf3->network, genesis_hash, Host::CapabilitiesFactory(), "/tmp/nw3", key3);
  nw2->start();
  nw3->start();

  EXPECT_HAPPENS({120s, 500ms}, [&](auto& ctx) {
    nw2->setPendingPeersToReady();
    nw3->setPendingPeersToReady();
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
  // we need to cleanup datadirs because we saved previous genesis_hash in db. And it is different after network_id
  // change
  CleanupDirs();
  {
    node_cfgs[0].network.network_id = 1;
    node_cfgs[1].network.network_id = 2;

    auto nodes = create_nodes(node_cfgs, true /*start*/);

    taraxa::thisThreadSleepForMilliSeconds(1000);
    EXPECT_EQ(nodes[0]->getNetwork()->getPeerCount(), 0);
    EXPECT_EQ(nodes[1]->getNetwork()->getPeerCount(), 0);
  }
}

// Test creates a DAG on one node and verifies that the second node syncs with it and that the resulting DAG on the
// other end is the same
TEST_F(NetworkTest, node_sync) {
  auto node_cfgs = make_node_cfgs<5>(2);
  auto node1 = create_nodes({node_cfgs[0]}, true /*start*/).front();
  // Stop PBFT manager
  node1->getPbftManager()->stop();

  // Allow node to start up
  taraxa::thisThreadSleepForMilliSeconds(1000);

  std::vector<std::pair<DagBlock, std::shared_ptr<Transaction>>> blks;
  // Generate DAG blocks
  const auto dag_genesis = node1->getConfig().chain.dag_genesis_block.getHash();
  const auto sk = node1->getSecretKey();
  const auto vrf_sk = node1->getVrfSecretKey();
  const auto estimation = node1->getTransactionManager()->estimateTransactionGas(g_signed_trx_samples[0], {});
  SortitionConfig vdf_config(node_cfgs[0].chain.sortition);

  auto propose_level = 1;
  const auto period_block_hash = node1->getDB()->getPeriodBlockHash(propose_level);
  vdf_sortition::VdfSortition vdf1(vdf_config, vrf_sk,
                                   VrfSortitionBase::makeVrfInput(propose_level, period_block_hash));
  vdf1.computeVdfSolution(vdf_config, dag_genesis.asBytes(), false);
  DagBlock blk1(dag_genesis, propose_level, {}, {g_signed_trx_samples[1]->getHash()}, {estimation}, vdf1, sk);

  propose_level = 2;
  vdf_sortition::VdfSortition vdf2(vdf_config, vrf_sk,
                                   VrfSortitionBase::makeVrfInput(propose_level, period_block_hash));
  vdf2.computeVdfSolution(vdf_config, blk1.getHash().asBytes(), false);
  DagBlock blk2(blk1.getHash(), propose_level, {}, {g_signed_trx_samples[2]->getHash()}, {estimation}, vdf2, sk);

  propose_level = 3;
  vdf_sortition::VdfSortition vdf3(vdf_config, vrf_sk,
                                   VrfSortitionBase::makeVrfInput(propose_level, period_block_hash));
  vdf3.computeVdfSolution(vdf_config, blk2.getHash().asBytes(), false);
  DagBlock blk3(blk2.getHash(), propose_level, {}, {g_signed_trx_samples[3]->getHash()}, {estimation}, vdf3, sk);

  propose_level = 4;
  vdf_sortition::VdfSortition vdf4(vdf_config, vrf_sk,
                                   VrfSortitionBase::makeVrfInput(propose_level, period_block_hash));
  vdf4.computeVdfSolution(vdf_config, blk3.getHash().asBytes(), false);
  DagBlock blk4(blk3.getHash(), propose_level, {}, {g_signed_trx_samples[4]->getHash()}, {estimation}, vdf4, sk);

  propose_level = 5;
  vdf_sortition::VdfSortition vdf5(vdf_config, vrf_sk,
                                   VrfSortitionBase::makeVrfInput(propose_level, period_block_hash));
  vdf5.computeVdfSolution(vdf_config, blk4.getHash().asBytes(), false);
  DagBlock blk5(blk4.getHash(), propose_level, {}, {g_signed_trx_samples[5]->getHash()}, {estimation}, vdf5, sk);

  propose_level = 6;
  vdf_sortition::VdfSortition vdf6(vdf_config, vrf_sk,
                                   VrfSortitionBase::makeVrfInput(propose_level, period_block_hash));
  vdf6.computeVdfSolution(vdf_config, blk5.getHash().asBytes(), false);
  DagBlock blk6(blk5.getHash(), propose_level, {blk4.getHash(), blk3.getHash()}, {g_signed_trx_samples[6]->getHash()},
                {estimation}, vdf6, sk);

  blks.push_back(std::make_pair(blk1, g_signed_trx_samples[1]));
  blks.push_back(std::make_pair(blk2, g_signed_trx_samples[2]));
  blks.push_back(std::make_pair(blk3, g_signed_trx_samples[3]));
  blks.push_back(std::make_pair(blk4, g_signed_trx_samples[4]));
  blks.push_back(std::make_pair(blk5, g_signed_trx_samples[5]));
  blks.push_back(std::make_pair(blk6, g_signed_trx_samples[6]));

  for (size_t i = 0; i < blks.size(); ++i) {
    node1->getTransactionManager()->insertValidatedTransactions({{blks[i].second, TransactionStatus::Verified}});
    EXPECT_EQ(node1->getDagBlockManager()->insertAndVerifyBlock(std::move(blks[i].first)),
              DagBlockManager::InsertAndVerifyBlockReturnType::InsertedAndVerified);
  }

  EXPECT_HAPPENS({30s, 500ms}, [&](auto& ctx) {
    WAIT_EXPECT_LT(ctx, 6, node1->getDagManager()->getNumVerticesInDag().first)
    WAIT_EXPECT_LT(ctx, 7, node1->getDagManager()->getNumEdgesInDag().first)
  });

  auto node2 = create_nodes({node_cfgs[1]}, true /*start*/).front();

  std::cout << "Waiting Sync..." << std::endl;
  EXPECT_HAPPENS({45s, 1500ms}, [&](auto& ctx) {
    WAIT_EXPECT_LT(ctx, 6, node2->getDagManager()->getNumVerticesInDag().first)
    WAIT_EXPECT_LT(ctx, 7, node2->getDagManager()->getNumEdgesInDag().first)
  });
}

// Test creates a PBFT chain on one node and verifies
// that the second node syncs with it and that the resulting
// chain on the other end is the same
TEST_F(NetworkTest, node_pbft_sync) {
  auto node_cfgs = make_node_cfgs<20>(2);
  auto node1 = create_nodes({node_cfgs[0]}, true /*start*/).front();

  // Stop PBFT manager and executor for syncing test
  node1->getPbftManager()->stop();

  auto db1 = node1->getDB();
  auto pbft_chain1 = node1->getPbftChain();

  auto dag_genesis = node1->getConfig().chain.dag_genesis_block.getHash();
  auto sk = node1->getSecretKey();
  auto vrf_sk = node1->getVrfSecretKey();
  SortitionConfig vdf_config(node_cfgs[0].chain.sortition);
  auto batch = db1->createWriteBatch();

  // generate first PBFT block sample
  blk_hash_t prev_block_hash(0);
  uint64_t period = 1;
  addr_t beneficiary(987);

  level_t level = 1;
  vdf_sortition::VdfSortition vdf1(vdf_config, vrf_sk, getRlpBytes(level));
  vdf1.computeVdfSolution(vdf_config, dag_genesis.asBytes(), false);
  DagBlock blk1(dag_genesis, 1, {}, {g_signed_trx_samples[0]->getHash(), g_signed_trx_samples[1]->getHash()}, {0, 0},
                vdf1, sk);
  std::vector<std::pair<std::shared_ptr<Transaction>, TransactionStatus>> txs1{
      {g_signed_trx_samples[0], TransactionStatus::Verified}, {g_signed_trx_samples[1], TransactionStatus::Verified}};

  node1->getTransactionManager()->insertValidatedTransactions(std::move(txs1));
  node1->getDagBlockManager()->insertAndVerifyBlock(DagBlock(blk1));

  dev::RLPStream order_stream(2);
  order_stream.appendList(1);
  order_stream << blk1.getHash();
  order_stream.appendList(2);
  order_stream << g_signed_trx_samples[0]->getHash() << g_signed_trx_samples[1]->getHash();

  PbftBlock pbft_block1(prev_block_hash, blk1.getHash(), dev::sha3(order_stream.out()), period, beneficiary,
                        node1->getSecretKey(), {});
  std::vector<std::shared_ptr<Vote>> votes_for_pbft_blk1;
  votes_for_pbft_blk1.emplace_back(
      node1->getPbftManager()->generateVote(pbft_block1.getBlockHash(), cert_vote_type, 1, 3));
  std::cout << "Generate 1 vote for first PBFT block" << std::endl;
  // Add cert votes in DB
  // Add PBFT block in DB

  PeriodData period_data1(std::make_shared<PbftBlock>(pbft_block1), {});
  period_data1.dag_blocks.push_back(blk1);
  period_data1.transactions.push_back(g_signed_trx_samples[0]);
  period_data1.transactions.push_back(g_signed_trx_samples[1]);

  db1->savePeriodData(period_data1, batch);
  // Update period_pbft_block in DB
  // Update pbft chain
  pbft_chain1->updatePbftChain(pbft_block1.getBlockHash());
  // Update PBFT chain head block
  blk_hash_t pbft_chain_head_hash = pbft_chain1->getHeadHash();
  std::string pbft_chain_head_str = pbft_chain1->getJsonStr();
  db1->addPbftHeadToBatch(pbft_chain_head_hash, pbft_chain_head_str, batch);
  db1->commitWriteBatch(batch);

  vec_blk_t order1;
  order1.push_back(blk1.getHash());
  {
    std::unique_lock dag_lock(node1->getDagManager()->getDagMutex());
    node1->getDagManager()->setDagBlockOrder(blk1.getHash(), level, order1);
  }

  uint64_t expect_pbft_chain_size = 1;
  EXPECT_EQ(node1->getPbftChain()->getPbftChainSize(), expect_pbft_chain_size);

  // generate second PBFT block sample
  prev_block_hash = pbft_block1.getBlockHash();

  level = 2;
  vdf_sortition::VdfSortition vdf2(vdf_config, vrf_sk, getRlpBytes(level));
  vdf2.computeVdfSolution(vdf_config, blk1.getHash().asBytes(), false);
  DagBlock blk2(blk1.getHash(), 2, {}, {g_signed_trx_samples[2]->getHash(), g_signed_trx_samples[3]->getHash()}, {0, 0},
                vdf2, sk);
  std::vector<std::pair<std::shared_ptr<Transaction>, TransactionStatus>> txs2{
      {g_signed_trx_samples[2], TransactionStatus::Verified}, {g_signed_trx_samples[3], TransactionStatus::Verified}};

  node1->getTransactionManager()->insertValidatedTransactions(std::move(txs2));
  node1->getDagBlockManager()->insertAndVerifyBlock(DagBlock(blk2));

  batch = db1->createWriteBatch();
  period = 2;
  beneficiary = addr_t(654);
  dev::RLPStream order_stream2(2);
  order_stream2.appendList(1);
  order_stream2 << blk2.getHash();
  order_stream2.appendList(2);
  order_stream2 << g_signed_trx_samples[2]->getHash() << g_signed_trx_samples[3]->getHash();
  PbftBlock pbft_block2(prev_block_hash, blk2.getHash(), dev::sha3(order_stream2.out()), period, beneficiary,
                        node1->getSecretKey(), {});
  std::vector<std::shared_ptr<Vote>> votes_for_pbft_blk2;
  votes_for_pbft_blk2.emplace_back(
      node1->getPbftManager()->generateVote(pbft_block2.getBlockHash(), cert_vote_type, 2, 3));
  std::cout << "Generate 1 vote for second PBFT block" << std::endl;
  // node1 put block2 into pbft chain and store into DB
  // Add cert votes in DB
  // Add PBFT block in DB

  std::cout << "B1 " << pbft_block2.getBlockHash() << std::endl;

  PeriodData period_data2(std::make_shared<PbftBlock>(pbft_block2), votes_for_pbft_blk1);
  period_data2.dag_blocks.push_back(blk2);
  period_data2.transactions.push_back(g_signed_trx_samples[2]);
  period_data2.transactions.push_back(g_signed_trx_samples[3]);
  db1->addLastBlockCertVotesToBatch(votes_for_pbft_blk2, batch);

  db1->savePeriodData(period_data2, batch);

  // Update pbft chain
  pbft_chain1->updatePbftChain(pbft_block2.getBlockHash());
  // Update PBFT chain head block
  pbft_chain_head_hash = pbft_chain1->getHeadHash();
  pbft_chain_head_str = pbft_chain1->getJsonStr();
  db1->addPbftHeadToBatch(pbft_chain_head_hash, pbft_chain_head_str, batch);
  db1->commitWriteBatch(batch);

  vec_blk_t order2;
  order2.push_back(blk2.getHash());
  {
    std::unique_lock dag_lock(node1->getDagManager()->getDagMutex());
    node1->getDagManager()->setDagBlockOrder(blk2.getHash(), level, order2);
  }

  expect_pbft_chain_size = 2;
  EXPECT_EQ(node1->getPbftChain()->getPbftChainSize(), expect_pbft_chain_size);

  auto node2 = create_nodes({node_cfgs[1]}, true /*start*/).front();
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
  auto node1 = create_nodes({node_cfgs[0]}, true /*start*/).front();

  // Stop PBFT manager and executor for syncing test
  node1->getPbftManager()->stop();

  auto db1 = node1->getDB();
  auto pbft_chain1 = node1->getPbftChain();

  auto dag_genesis = node1->getConfig().chain.dag_genesis_block.getHash();
  auto sk = node1->getSecretKey();
  auto vrf_sk = node1->getVrfSecretKey();
  SortitionConfig vdf_config(node_cfgs[0].chain.sortition);
  auto batch = db1->createWriteBatch();

  // generate first PBFT block sample
  blk_hash_t prev_block_hash(0);
  uint64_t period = 1;
  addr_t beneficiary(876);
  level_t level = 1;
  vdf_sortition::VdfSortition vdf1(vdf_config, vrf_sk, getRlpBytes(level));
  vdf1.computeVdfSolution(vdf_config, dag_genesis.asBytes(), false);
  DagBlock blk1(dag_genesis, 1, {}, {g_signed_trx_samples[0]->getHash(), g_signed_trx_samples[1]->getHash()}, {0, 0},
                vdf1, sk);
  std::vector<std::pair<std::shared_ptr<Transaction>, TransactionStatus>> tr1{
      {g_signed_trx_samples[0], TransactionStatus::Verified}, {g_signed_trx_samples[1], TransactionStatus::Verified}};

  node1->getTransactionManager()->insertValidatedTransactions(std::move(tr1));
  node1->getDagBlockManager()->insertAndVerifyBlock(DagBlock(blk1));

  dev::RLPStream order_stream(2);
  order_stream.appendList(1);
  order_stream << blk1.getHash();
  order_stream.appendList(2);
  order_stream << g_signed_trx_samples[0]->getHash() << g_signed_trx_samples[1]->getHash();

  PbftBlock pbft_block1(prev_block_hash, blk1.getHash(), dev::sha3(order_stream.out()), period, beneficiary,
                        node1->getSecretKey(), {});
  std::vector<std::shared_ptr<Vote>> votes_for_pbft_blk1;
  votes_for_pbft_blk1.emplace_back(
      node1->getPbftManager()->generateVote(pbft_block1.getBlockHash(), cert_vote_type, 1, 3));
  std::cout << "Generate 1 vote for first PBFT block" << std::endl;
  // Add cert votes in DB
  // Add PBFT block in DB

  PeriodData period_data1(std::make_shared<PbftBlock>(pbft_block1), {});
  period_data1.dag_blocks.push_back(blk1);
  period_data1.transactions.push_back(g_signed_trx_samples[0]);
  period_data1.transactions.push_back(g_signed_trx_samples[1]);

  db1->savePeriodData(period_data1, batch);
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
  level = 2;
  vdf_sortition::VdfSortition vdf2(vdf_config, vrf_sk, getRlpBytes(level));
  vdf2.computeVdfSolution(vdf_config, blk1.getHash().asBytes(), false);
  DagBlock blk2(blk1.getHash(), 2, {}, {g_signed_trx_samples[2]->getHash(), g_signed_trx_samples[3]->getHash()}, {0, 0},
                vdf2, sk);
  std::vector<std::pair<std::shared_ptr<Transaction>, TransactionStatus>> tr2{
      {g_signed_trx_samples[2], TransactionStatus::Verified}, {g_signed_trx_samples[3], TransactionStatus::Verified}};

  node1->getTransactionManager()->insertValidatedTransactions(std::move(tr2));
  node1->getDagBlockManager()->insertAndVerifyBlock(DagBlock(blk2));

  batch = db1->createWriteBatch();
  period = 2;
  beneficiary = addr_t(654);

  dev::RLPStream order_stream2(2);
  order_stream2.appendList(1);
  order_stream2 << blk2.getHash();
  order_stream2.appendList(2);
  order_stream2 << g_signed_trx_samples[2]->getHash() << g_signed_trx_samples[3]->getHash();

  PbftBlock pbft_block2(prev_block_hash, blk2.getHash(), dev::sha3(order_stream2.out()), period, beneficiary,
                        node1->getSecretKey(), {});
  std::cout << "Use fake votes for the second PBFT block" << std::endl;
  // node1 put block2 into pbft chain and use fake votes storing into DB (malicious player)
  // Add fake votes in DB
  // Add PBFT block in DB

  PeriodData period_data2(std::make_shared<PbftBlock>(pbft_block2), votes_for_pbft_blk1);
  period_data2.dag_blocks.push_back(blk2);
  period_data2.transactions.push_back(g_signed_trx_samples[2]);
  period_data2.transactions.push_back(g_signed_trx_samples[3]);

  db1->savePeriodData(period_data2, batch);
  db1->addLastBlockCertVotesToBatch(votes_for_pbft_blk1, batch);
  // Update pbft chain
  pbft_chain1->updatePbftChain(pbft_block2.getBlockHash());
  // Update PBFT chain head block
  pbft_chain_head_hash = pbft_chain1->getHeadHash();
  pbft_chain_head_str = pbft_chain1->getJsonStr();
  db1->addPbftHeadToBatch(pbft_chain_head_hash, pbft_chain_head_str, batch);
  db1->commitWriteBatch(batch);
  expect_pbft_chain_size = 2;
  EXPECT_EQ(node1->getPbftChain()->getPbftChainSize(), expect_pbft_chain_size);

  auto node2 = create_nodes({node_cfgs[1]}, true /*start*/).front();
  std::cout << "Waiting Sync for max 1 minutes..." << std::endl;
  uint64_t sync_pbft_chain_size = 1;
  for (int i = 0; i < 600; i++) {
    if (node2->getPbftManager()->pbftSyncingPeriod() >= sync_pbft_chain_size) {
      break;
    }
    taraxa::thisThreadSleepForMilliSeconds(100);
  }
  EXPECT_EQ(node2->getPbftManager()->pbftSyncingPeriod(), sync_pbft_chain_size);
}

// Test PBFT next votes sycning when node is behind of PBFT round with peer
TEST_F(NetworkTest, pbft_next_votes_sync_in_behind_round) {
  auto node_cfgs = make_node_cfgs<20>(2);
  auto node1 = create_nodes({node_cfgs[0]}, true /*start*/).front();

  // Stop PBFT manager, that will place vote
  std::shared_ptr<PbftManager> pbft_mgr1 = node1->getPbftManager();
  pbft_mgr1->stop();

  // Generate 3 next votes
  std::vector<std::shared_ptr<Vote>> next_votes;
  PbftVoteTypes type = next_vote_type;
  uint64_t round = 1;
  size_t step = 5;
  for (auto i = 0; i < 3; i++) {
    blk_hash_t voted_pbft_block_hash(i % 2);  // Next votes could vote on 2 values
    auto vote = pbft_mgr1->generateVote(voted_pbft_block_hash, type, round, step + i);
    vote->calculateWeight(1, 1, 1);
    next_votes.push_back(std::move(vote));
  }

  // Update next votes bundle and set PBFT round
  auto pbft_2t_plus_1 = 1;
  node1->getNextVotesManager()->updateNextVotes(next_votes, pbft_2t_plus_1);
  pbft_mgr1->setPbftRound(2);  // Make sure node2 PBFT round is less than node1

  auto node2 = create_nodes({node_cfgs[1]}, true /*start*/).front();
  // Stop PBFT manager, that will place vote
  std::shared_ptr<PbftManager> pbft_mgr2 = node2->getPbftManager();
  pbft_mgr2->stop();
  pbft_mgr2->setPbftRound(1);  // Make sure node2 PBFT round is less than node1
  node2->getVoteManager()->clearUnverifiedVotesTable();

  // Wait node1 and node2 connect to each other
  EXPECT_HAPPENS({10s, 100ms}, [&](auto& ctx) {
    WAIT_EXPECT_EQ(ctx, node1->getNetwork()->getPeerCount(), 1)
    WAIT_EXPECT_EQ(ctx, node2->getNetwork()->getPeerCount(), 1)
  });

  // Node2 wait for getting votes from node1 by sending status packet
  EXPECT_HAPPENS({10s, 500ms}, [&](auto& ctx) {
    WAIT_EXPECT_EQ(ctx, node2->getVoteManager()->getUnverifiedVotesSize(), next_votes.size())
  });
}

// Test PBFT next votes sycning when nodes stay at same PBFT round, but node2 has less previous round next votes size
TEST_F(NetworkTest, pbft_next_votes_sync_in_same_round_1) {
  auto pbft_previous_round = 1;

  auto node_cfgs = make_node_cfgs<20>(2);
  std::vector<std::shared_ptr<FullNode>> nodes;
  for (auto i(0); i < 2; i++) {
    nodes.emplace_back(std::make_shared<FullNode>(node_cfgs[i]));
    nodes.back()->start();
    // Stop PBFT manager, that will place vote
    nodes.back()->getPbftManager()->stop();
  }
  EXPECT_TRUE(wait_connect(nodes));

  auto& node1 = nodes[0];
  auto& node2 = nodes[1];

  // Clear next votes components
  auto node1_next_votes_mgr = node1->getNextVotesManager();
  auto node2_next_votes_mgr = node2->getNextVotesManager();
  node1_next_votes_mgr->clear();
  node2_next_votes_mgr->clear();

  auto pbft_mgr1 = node1->getPbftManager();
  auto pbft_mgr2 = node2->getPbftManager();
  auto node1_pbft_2t_plus_1 = pbft_mgr1->getTwoTPlusOne();
  EXPECT_EQ(node1_pbft_2t_plus_1, 1);
  auto node2_pbft_2t_plus_1 = pbft_mgr2->getTwoTPlusOne();
  EXPECT_EQ(node2_pbft_2t_plus_1, 1);

  // Generate 2 next votes for noode1
  std::vector<std::shared_ptr<Vote>> next_votes1;
  uint64_t round = 1;
  size_t step = 5;
  PbftVoteTypes type = next_vote_type;
  for (uint64_t i = 0; i < 2; i++) {
    blk_hash_t voted_pbft_block_hash1(i);  // Next votes could vote on 2 values
    auto vote = pbft_mgr1->generateVote(voted_pbft_block_hash1, type, round, step);
    vote->calculateWeight(1, 1, 1);
    next_votes1.push_back(std::move(vote));
  }

  // Update node1 next votes bundle
  node1_next_votes_mgr->updateNextVotes(next_votes1, node1_pbft_2t_plus_1);
  EXPECT_EQ(node1_next_votes_mgr->getNextVotesWeight(), next_votes1.size());

  // Generate 1 same next votes with node1, voted same value on NULL_BLOCK_HASH
  blk_hash_t voted_pbft_block_hash2(0);
  auto vote1 = pbft_mgr1->generateVote(voted_pbft_block_hash2, type, round, step);
  vote1->calculateWeight(1, 1, 1);
  std::vector<std::shared_ptr<Vote>> next_votes2{vote1};

  // Update node2 next votes bundle
  node2_next_votes_mgr->updateNextVotes(next_votes2, node2_pbft_2t_plus_1);
  EXPECT_EQ(node2_next_votes_mgr->getNextVotesWeight(), next_votes2.size());

  // Set both node1 and node2 pbft manager round to 2
  pbft_mgr1->setPbftRound(2);
  pbft_mgr2->setPbftRound(2);

  // Set PBFT previous round 2t+1, sortition threshold, DPOS period and DPOS total votes count for syncing
  auto db = node2->getDB();
  auto batch = db->createWriteBatch();
  db->addPbft2TPlus1ToBatch(pbft_previous_round, node2_pbft_2t_plus_1, batch);
  db->addPbftMgrPreviousRoundStatus(PbftMgrPreviousRoundStatus::PreviousRoundSortitionThreshold, 1, batch);
  db->addPbftMgrPreviousRoundStatus(PbftMgrPreviousRoundStatus::PreviousRoundDposPeriod, 0, batch);
  db->addPbftMgrPreviousRoundStatus(PbftMgrPreviousRoundStatus::PreviousRoundDposTotalVotesCount, 1, batch);
  db->commitWriteBatch(batch);

  auto expect_size = next_votes1.size();
  EXPECT_HAPPENS({30s, 500ms},
                 [&](auto& ctx) { WAIT_EXPECT_EQ(ctx, node2_next_votes_mgr->getNextVotesWeight(), expect_size); });
}

// Test PBFT next votes sycning when nodes stay at same PBFT round, node1 and node2 have different previous round next
// votes set
TEST_F(NetworkTest, pbft_next_votes_sync_in_same_round_2) {
  auto pbft_previous_round = 1;

  auto node_cfgs = make_node_cfgs<20>(2);
  std::vector<std::shared_ptr<FullNode>> nodes;
  for (auto i(0); i < 2; i++) {
    nodes.emplace_back(std::make_shared<FullNode>(node_cfgs[i]));
    nodes.back()->start();
    // Stop PBFT manager, that will place vote
    nodes.back()->getPbftManager()->stop();
  }
  EXPECT_TRUE(wait_connect(nodes));

  auto& node1 = nodes[0];
  auto& node2 = nodes[1];

  // Clear next votes components
  auto node1_next_votes_mgr = node1->getNextVotesManager();
  auto node2_next_votes_mgr = node2->getNextVotesManager();
  node1_next_votes_mgr->clear();
  node2_next_votes_mgr->clear();

  auto pbft_mgr1 = node1->getPbftManager();
  auto pbft_mgr2 = node2->getPbftManager();
  auto node1_pbft_2t_plus_1 = pbft_mgr1->getTwoTPlusOne();
  EXPECT_EQ(node1_pbft_2t_plus_1, 1);
  auto node2_pbft_2t_plus_1 = pbft_mgr2->getTwoTPlusOne();
  EXPECT_EQ(node2_pbft_2t_plus_1, 1);

  // Node1 generate 1 next vote voted at NULL_BLOCK_HASH
  blk_hash_t voted_pbft_block_hash1(blk_hash_t(0));
  PbftVoteTypes type = next_vote_type;
  uint64_t round = 1;
  size_t step = 5;
  auto vote1 = pbft_mgr1->generateVote(voted_pbft_block_hash1, type, round, step);
  vote1->calculateWeight(1, 1, 1);
  std::vector<std::shared_ptr<Vote>> next_votes1{vote1};

  // Update node1 next votes bundle
  node1_next_votes_mgr->updateNextVotes(next_votes1, node1_pbft_2t_plus_1);
  EXPECT_EQ(node1_next_votes_mgr->getNextVotesWeight(), next_votes1.size());

  // Node1 generate 1 different next vote for node2, because node2 is not delegated
  blk_hash_t voted_pbft_block_hash2("1234567890000000000000000000000000000000000000000000000000000000");
  auto vote2 = pbft_mgr1->generateVote(voted_pbft_block_hash2, type, round, step);
  vote2->calculateWeight(1, 1, 1);
  std::vector<std::shared_ptr<Vote>> next_votes2{vote2};

  // Update node2 next votes bundle
  node2_next_votes_mgr->updateNextVotes(next_votes2, node2_pbft_2t_plus_1);
  EXPECT_EQ(node2_next_votes_mgr->getNextVotesWeight(), next_votes2.size());

  // Set both node1 and node2 pbft manager round to 2
  pbft_mgr1->setPbftRound(2);
  pbft_mgr2->setPbftRound(2);

  // Set node2 PBFT previous round 2t+1, sortition threshold, DPOS period and DPOS total votes count for syncing
  auto node2_db = node2->getDB();
  auto batch = node2_db->createWriteBatch();
  node2_db->addPbft2TPlus1ToBatch(pbft_previous_round, node2_pbft_2t_plus_1, batch);
  node2_db->addPbftMgrPreviousRoundStatus(PbftMgrPreviousRoundStatus::PreviousRoundSortitionThreshold, 1, batch);
  node2_db->addPbftMgrPreviousRoundStatus(PbftMgrPreviousRoundStatus::PreviousRoundDposPeriod, 0, batch);
  node2_db->addPbftMgrPreviousRoundStatus(PbftMgrPreviousRoundStatus::PreviousRoundDposTotalVotesCount, 1, batch);
  node2_db->commitWriteBatch(batch);

  std::shared_ptr<Network> nw1 = node1->getNetwork();
  std::shared_ptr<Network> nw2 = node2->getNetwork();

  // Node1 broadcast next votes1 to node2
  nw1->getSpecificHandler<network::tarcap::VotesSyncPacketHandler>()->broadcastPreviousRoundNextVotesBundle();

  auto node2_expect_size = next_votes1.size() + next_votes2.size();
  EXPECT_HAPPENS({5s, 100ms},
                 [&](auto& ctx) { WAIT_EXPECT_EQ(ctx, node2_next_votes_mgr->getNextVotesWeight(), node2_expect_size) });

  // Expect node1 print out "ERROR: Cannot get PBFT 2t+1 in PBFT round 0"
  EXPECT_EQ(node1_next_votes_mgr->getNextVotesWeight(), next_votes1.size());

  // Set node1 PBFT previous round 2t+1, sortition threshold, DPOS period and DPOS total votes count for syncing
  auto node1_db = node1->getDB();
  batch = node1_db->createWriteBatch();
  node1_db->addPbft2TPlus1ToBatch(pbft_previous_round, node1_pbft_2t_plus_1, batch);
  node1_db->addPbftMgrPreviousRoundStatus(PbftMgrPreviousRoundStatus::PreviousRoundSortitionThreshold, 1, batch);
  node1_db->addPbftMgrPreviousRoundStatus(PbftMgrPreviousRoundStatus::PreviousRoundDposPeriod, 0, batch);
  node1_db->addPbftMgrPreviousRoundStatus(PbftMgrPreviousRoundStatus::PreviousRoundDposTotalVotesCount, 1, batch);
  node1_db->commitWriteBatch(batch);

  // Node2 broadcast updated next votes to node1
  nw2->getSpecificHandler<network::tarcap::VotesSyncPacketHandler>()->broadcastPreviousRoundNextVotesBundle();

  auto node1_expect_size = next_votes1.size() + next_votes2.size();
  EXPECT_HAPPENS({5s, 100ms},
                 [&](auto& ctx) { WAIT_EXPECT_EQ(ctx, node1_next_votes_mgr->getNextVotesWeight(), node1_expect_size) });
}

// Test creates a DAG on one node and verifies
// that the second node syncs with it and that the resulting
// DAG on the other end is the same
// Unlike the previous tests, this DAG contains blocks with transactions
// and verifies that the sync containing transactions is successful
TEST_F(NetworkTest, node_sync_with_transactions) {
  auto node_cfgs = make_node_cfgs<5>(2);
  auto node1 = create_nodes({node_cfgs[0]}, true /*start*/).front();

  std::vector<DagBlock> blks;
  // Generate DAG blocks
  const auto dag_genesis = node1->getConfig().chain.dag_genesis_block.getHash();
  const auto sk = node1->getSecretKey();
  const auto vrf_sk = node1->getVrfSecretKey();
  const auto estimation = node1->getTransactionManager()->estimateTransactionGas(g_signed_trx_samples[0], {});

  SortitionConfig vdf_config(node_cfgs[0].chain.sortition);
  auto propose_level = 1;
  const auto period_block_hash = node1->getDB()->getPeriodBlockHash(propose_level);
  vdf_sortition::VdfSortition vdf1(vdf_config, vrf_sk,
                                   VrfSortitionBase::makeVrfInput(propose_level, period_block_hash));
  vdf1.computeVdfSolution(vdf_config, dag_genesis.asBytes(), false);
  DagBlock blk1(dag_genesis, propose_level, {},
                {g_signed_trx_samples[0]->getHash(), g_signed_trx_samples[1]->getHash()}, {estimation, estimation},
                vdf1, sk);
  std::vector<std::pair<std::shared_ptr<Transaction>, TransactionStatus>> tr1{
      {g_signed_trx_samples[0], TransactionStatus::Verified}, {g_signed_trx_samples[1], TransactionStatus::Verified}};

  propose_level = 2;
  vdf_sortition::VdfSortition vdf2(vdf_config, vrf_sk,
                                   VrfSortitionBase::makeVrfInput(propose_level, period_block_hash));
  vdf2.computeVdfSolution(vdf_config, blk1.getHash().asBytes(), false);
  DagBlock blk2(blk1.getHash(), propose_level, {}, {g_signed_trx_samples[2]->getHash()}, {estimation}, vdf2, sk);
  std::vector<std::pair<std::shared_ptr<Transaction>, TransactionStatus>> tr2{
      {g_signed_trx_samples[2], TransactionStatus::Verified}};

  propose_level = 3;
  vdf_sortition::VdfSortition vdf3(vdf_config, vrf_sk,
                                   VrfSortitionBase::makeVrfInput(propose_level, period_block_hash));
  vdf3.computeVdfSolution(vdf_config, blk2.getHash().asBytes(), false);
  DagBlock blk3(blk2.getHash(), propose_level, {}, {g_signed_trx_samples[3]->getHash()}, {estimation}, vdf3, sk);
  std::vector<std::pair<std::shared_ptr<Transaction>, TransactionStatus>> tr3{
      {g_signed_trx_samples[3], TransactionStatus::Verified}};

  propose_level = 4;
  vdf_sortition::VdfSortition vdf4(vdf_config, vrf_sk,
                                   VrfSortitionBase::makeVrfInput(propose_level, period_block_hash));
  vdf4.computeVdfSolution(vdf_config, blk3.getHash().asBytes(), false);
  DagBlock blk4(blk3.getHash(), propose_level, {}, {g_signed_trx_samples[4]->getHash()}, {estimation}, vdf4, sk);
  std::vector<std::pair<std::shared_ptr<Transaction>, TransactionStatus>> tr4{
      {g_signed_trx_samples[4], TransactionStatus::Verified}};

  propose_level = 5;
  vdf_sortition::VdfSortition vdf5(vdf_config, vrf_sk,
                                   VrfSortitionBase::makeVrfInput(propose_level, period_block_hash));
  vdf5.computeVdfSolution(vdf_config, blk4.getHash().asBytes(), false);
  DagBlock blk5(blk4.getHash(), propose_level, {},
                {g_signed_trx_samples[5]->getHash(), g_signed_trx_samples[6]->getHash(),
                 g_signed_trx_samples[7]->getHash(), g_signed_trx_samples[8]->getHash()},
                {estimation, estimation, estimation, estimation}, vdf5, sk);
  std::vector<std::pair<std::shared_ptr<Transaction>, TransactionStatus>> tr5{
      {g_signed_trx_samples[5], TransactionStatus::Verified},
      {g_signed_trx_samples[6], TransactionStatus::Verified},
      {g_signed_trx_samples[7], TransactionStatus::Verified},
      {g_signed_trx_samples[8], TransactionStatus::Verified}};

  propose_level = 6;
  vdf_sortition::VdfSortition vdf6(vdf_config, vrf_sk,
                                   VrfSortitionBase::makeVrfInput(propose_level, period_block_hash));
  vdf6.computeVdfSolution(vdf_config, blk5.getHash().asBytes(), false);
  DagBlock blk6(blk5.getHash(), propose_level, {blk4.getHash(), blk3.getHash()}, {g_signed_trx_samples[9]->getHash()},
                {estimation}, vdf6, sk);
  std::vector<std::pair<std::shared_ptr<Transaction>, TransactionStatus>> tr6{
      {g_signed_trx_samples[9], TransactionStatus::Verified}};

  node1->getTransactionManager()->insertValidatedTransactions(std::move(tr1));
  EXPECT_EQ(node1->getDagBlockManager()->insertAndVerifyBlock(std::move(blk1)),
            DagBlockManager::InsertAndVerifyBlockReturnType::InsertedAndVerified);
  node1->getTransactionManager()->insertValidatedTransactions(std::move(tr2));
  EXPECT_EQ(node1->getDagBlockManager()->insertAndVerifyBlock(std::move(blk2)),
            DagBlockManager::InsertAndVerifyBlockReturnType::InsertedAndVerified);
  node1->getTransactionManager()->insertValidatedTransactions(std::move(tr3));
  EXPECT_EQ(node1->getDagBlockManager()->insertAndVerifyBlock(std::move(blk3)),
            DagBlockManager::InsertAndVerifyBlockReturnType::InsertedAndVerified);
  node1->getTransactionManager()->insertValidatedTransactions(std::move(tr4));
  EXPECT_EQ(node1->getDagBlockManager()->insertAndVerifyBlock(std::move(blk4)),
            DagBlockManager::InsertAndVerifyBlockReturnType::InsertedAndVerified);
  node1->getTransactionManager()->insertValidatedTransactions(std::move(tr5));
  EXPECT_EQ(node1->getDagBlockManager()->insertAndVerifyBlock(std::move(blk5)),
            DagBlockManager::InsertAndVerifyBlockReturnType::InsertedAndVerified);
  node1->getTransactionManager()->insertValidatedTransactions(std::move(tr6));
  EXPECT_EQ(node1->getDagBlockManager()->insertAndVerifyBlock(std::move(blk6)),
            DagBlockManager::InsertAndVerifyBlockReturnType::InsertedAndVerified);
  // To make sure blocks are stored before starting node 2
  taraxa::thisThreadSleepForMilliSeconds(1000);

  EXPECT_GT(node1->getDagManager()->getNumVerticesInDag().first, 6);
  EXPECT_GT(node1->getDagManager()->getNumEdgesInDag().first, 7);

  auto node2 = create_nodes({node_cfgs[1]}, true /*start*/).front();

  std::cout << "Waiting Sync for up to 20000 milliseconds ..." << std::endl;
  wait({20s, 100ms}, [&](auto& ctx) {
    WAIT_EXPECT_EQ(ctx, node2->getDagManager()->getNumVerticesInDag().first,
                   node1->getDagManager()->getNumVerticesInDag().first)
  });
  EXPECT_EQ(node2->getDagManager()->getNumEdgesInDag().first, node1->getDagManager()->getNumEdgesInDag().first);
}

// Test creates a complex DAG on one node and verifies
// that the second node syncs with it and that the resulting
// DAG on the other end is the same
TEST_F(NetworkTest, node_sync2) {
  auto node_cfgs = make_node_cfgs<5>(2);
  auto node1 = create_nodes({node_cfgs[0]}, true /*start*/).front();

  std::vector<DagBlock> blks;
  // Generate DAG blocks
  const auto dag_genesis = node1->getConfig().chain.dag_genesis_block.getHash();
  const auto sk = node1->getSecretKey();
  const auto vrf_sk = node1->getVrfSecretKey();
  const SortitionConfig vdf_config(node_cfgs[0].chain.sortition);
  const auto transactions = samples::createSignedTrxSamples(0, 25, sk);
  const auto estimation = node1->getTransactionManager()->estimateTransactionGas(transactions[0], {});
  const auto estimations = {estimation, estimation};
  // DAG block1
  auto propose_level = 1;
  const auto period_block_hash = node1->getDB()->getPeriodBlockHash(propose_level);
  vdf_sortition::VdfSortition vdf1(vdf_config, vrf_sk,
                                   VrfSortitionBase::makeVrfInput(propose_level, period_block_hash));
  vdf1.computeVdfSolution(vdf_config, dag_genesis.asBytes(), false);
  DagBlock blk1(dag_genesis, propose_level, {}, {transactions[0]->getHash(), transactions[1]->getHash()}, estimations,
                vdf1, sk);
  SharedTransactions tr1({transactions[0], transactions[1]});
  // DAG block2
  propose_level = 1;
  vdf_sortition::VdfSortition vdf2(vdf_config, vrf_sk,
                                   VrfSortitionBase::makeVrfInput(propose_level, period_block_hash));
  vdf2.computeVdfSolution(vdf_config, dag_genesis.asBytes(), false);
  DagBlock blk2(dag_genesis, propose_level, {}, {transactions[2]->getHash(), transactions[3]->getHash()}, estimations,
                vdf2, sk);
  SharedTransactions tr2({transactions[2], transactions[3]});
  // DAG block3
  propose_level = 2;
  vdf_sortition::VdfSortition vdf3(vdf_config, vrf_sk,
                                   VrfSortitionBase::makeVrfInput(propose_level, period_block_hash));
  vdf3.computeVdfSolution(vdf_config, blk1.getHash().asBytes(), false);
  DagBlock blk3(blk1.getHash(), propose_level, {}, {transactions[4]->getHash(), transactions[5]->getHash()},
                estimations, vdf3, sk);
  SharedTransactions tr3({transactions[4], transactions[5]});
  // DAG block4
  propose_level = 3;
  vdf_sortition::VdfSortition vdf4(vdf_config, vrf_sk,
                                   VrfSortitionBase::makeVrfInput(propose_level, period_block_hash));
  vdf4.computeVdfSolution(vdf_config, blk3.getHash().asBytes(), false);
  DagBlock blk4(blk3.getHash(), propose_level, {}, {transactions[6]->getHash(), transactions[7]->getHash()},
                estimations, vdf4, sk);
  SharedTransactions tr4({transactions[6], transactions[7]});
  // DAG block5
  propose_level = 2;
  vdf_sortition::VdfSortition vdf5(vdf_config, vrf_sk,
                                   VrfSortitionBase::makeVrfInput(propose_level, period_block_hash));
  vdf5.computeVdfSolution(vdf_config, blk2.getHash().asBytes(), false);
  DagBlock blk5(blk2.getHash(), propose_level, {}, {transactions[8]->getHash(), transactions[9]->getHash()},
                estimations, vdf5, sk);
  SharedTransactions tr5({transactions[8], transactions[9]});
  // DAG block6
  propose_level = 2;
  vdf_sortition::VdfSortition vdf6(vdf_config, vrf_sk,
                                   VrfSortitionBase::makeVrfInput(propose_level, period_block_hash));
  vdf6.computeVdfSolution(vdf_config, blk1.getHash().asBytes(), false);
  DagBlock blk6(blk1.getHash(), propose_level, {}, {transactions[10]->getHash(), transactions[11]->getHash()},
                estimations, vdf6, sk);
  SharedTransactions tr6({transactions[10], transactions[11]});
  // DAG block7
  propose_level = 3;
  vdf_sortition::VdfSortition vdf7(vdf_config, vrf_sk,
                                   VrfSortitionBase::makeVrfInput(propose_level, period_block_hash));
  vdf7.computeVdfSolution(vdf_config, blk6.getHash().asBytes(), false);
  DagBlock blk7(blk6.getHash(), propose_level, {}, {transactions[12]->getHash(), transactions[13]->getHash()},
                estimations, vdf7, sk);
  SharedTransactions tr7({transactions[12], transactions[13]});
  // DAG block8
  propose_level = 4;
  vdf_sortition::VdfSortition vdf8(vdf_config, vrf_sk,
                                   VrfSortitionBase::makeVrfInput(propose_level, period_block_hash));
  vdf8.computeVdfSolution(vdf_config, blk1.getHash().asBytes(), false);
  DagBlock blk8(blk1.getHash(), propose_level, {blk7.getHash()},
                {transactions[14]->getHash(), transactions[15]->getHash()}, estimations, vdf8, sk);
  SharedTransactions tr8({transactions[14], transactions[15]});
  // DAG block9
  propose_level = 2;
  vdf_sortition::VdfSortition vdf9(vdf_config, vrf_sk,
                                   VrfSortitionBase::makeVrfInput(propose_level, period_block_hash));
  vdf9.computeVdfSolution(vdf_config, blk1.getHash().asBytes(), false);
  DagBlock blk9(blk1.getHash(), propose_level, {}, {transactions[16]->getHash(), transactions[17]->getHash()},
                estimations, vdf9, sk);
  SharedTransactions tr9({transactions[16], transactions[17]});
  // DAG block10
  propose_level = 5;
  vdf_sortition::VdfSortition vdf10(vdf_config, vrf_sk,
                                    VrfSortitionBase::makeVrfInput(propose_level, period_block_hash));
  vdf10.computeVdfSolution(vdf_config, blk8.getHash().asBytes(), false);
  DagBlock blk10(blk8.getHash(), propose_level, {}, {transactions[18]->getHash(), transactions[19]->getHash()},
                 estimations, vdf10, sk);
  SharedTransactions tr10({transactions[18], transactions[19]});
  // DAG block11
  propose_level = 3;
  vdf_sortition::VdfSortition vdf11(vdf_config, vrf_sk,
                                    VrfSortitionBase::makeVrfInput(propose_level, period_block_hash));
  vdf11.computeVdfSolution(vdf_config, blk3.getHash().asBytes(), false);
  DagBlock blk11(blk3.getHash(), propose_level, {}, {transactions[20]->getHash(), transactions[21]->getHash()},
                 estimations, vdf11, sk);
  SharedTransactions tr11({transactions[20], transactions[21]});
  // DAG block12
  propose_level = 3;
  vdf_sortition::VdfSortition vdf12(vdf_config, vrf_sk,
                                    VrfSortitionBase::makeVrfInput(propose_level, period_block_hash));
  vdf12.computeVdfSolution(vdf_config, blk5.getHash().asBytes(), false);
  DagBlock blk12(blk5.getHash(), propose_level, {}, {transactions[22]->getHash(), transactions[23]->getHash()},
                 estimations, vdf12, sk);
  SharedTransactions tr12({transactions[22], transactions[23]});

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

  std::vector<SharedTransactions> trxs;
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
    std::vector<std::pair<std::shared_ptr<Transaction>, TransactionStatus>> ver_trxs;
    for (auto t : trxs[i]) ver_trxs.push_back({t, TransactionStatus::Verified});
    node1->getTransactionManager()->insertValidatedTransactions(std::move(ver_trxs));
    node1->getDagBlockManager()->insertAndVerifyBlock(std::move(blks[i]));
  }

  auto node2 = create_nodes({node_cfgs[1]}, true /*start*/).front();

  EXPECT_HAPPENS({10s, 100ms}, [&](auto& ctx) {
    WAIT_EXPECT_LT(ctx, 12, node1->getDagManager()->getNumVerticesInDag().first)
    WAIT_EXPECT_LT(ctx, 12, node1->getDagManager()->getNumEdgesInDag().first)
  });

  EXPECT_HAPPENS({50s, 300ms}, [&](auto& ctx) {
    WAIT_EXPECT_EQ(ctx, node1->getDagManager()->getNumVerticesInDag().first,
                   node2->getDagManager()->getNumVerticesInDag().first)
    WAIT_EXPECT_EQ(ctx, node1->getDagManager()->getNumEdgesInDag().first,
                   node2->getDagManager()->getNumEdgesInDag().first)
  });
}

// Test creates new transactions on one node and verifies
// that the second node receives the transactions
TEST_F(NetworkTest, node_transaction_sync) {
  auto node_cfgs = make_node_cfgs(2);
  auto nodes = launch_nodes(node_cfgs);
  auto& node1 = nodes[0];
  auto& node2 = nodes[1];

  std::vector<std::pair<std::shared_ptr<Transaction>, TransactionStatus>> ver_trxs;
  for (auto t : *g_signed_trx_samples) ver_trxs.push_back({t, TransactionStatus::Verified});
  node1->getTransactionManager()->insertValidatedTransactions(std::move(ver_trxs));

  std::cout << "Waiting Sync for 2000 milliseconds ..." << std::endl;
  taraxa::thisThreadSleepForMilliSeconds(2000);

  for (auto const& t : *g_signed_trx_samples) {
    EXPECT_TRUE(node2->getTransactionManager()->getTransaction(t->getHash()) != nullptr);
    if (node2->getTransactionManager()->getTransaction(t->getHash()) != nullptr) {
      EXPECT_EQ(*t, *node2->getTransactionManager()->getTransaction(t->getHash()));
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

  int num_of_trxs = 50;
  const auto trxs = samples::createSignedTrxSamples(0, num_of_trxs, g_secret);
  for (auto i = 0; i < num_of_trxs; ++i) {
    nodes[distNodes(rng)]->getTransactionManager()->insertValidatedTransactions(
        {{trxs[i], TransactionStatus::Verified}});
    thisThreadSleepForMilliSeconds(distTransactions(rng));
  }
  ASSERT_EQ(num_of_trxs, 50);  // 50 transactions

  std::cout << "Waiting Sync ..." << std::endl;

  wait({60s, 100ms}, [&](auto& ctx) {
    // Check 4 nodes syncing
    for (int j = 1; j < numberOfNodes - 1; j++) {
      WAIT_EXPECT_EQ(ctx, nodes[j]->getDagManager()->getNumVerticesInDag().first,
                     nodes[0]->getDagManager()->getNumVerticesInDag().first);
      WAIT_EXPECT_EQ(ctx, nodes[j]->getPbftChain()->getPbftChainSizeExcludingEmptyPbftBlocks(),
                     nodes[0]->getPbftChain()->getPbftChainSizeExcludingEmptyPbftBlocks());
    }
  });

  bool dag_synced = true;
  auto node0_vertices = nodes[0]->getDagManager()->getNumVerticesInDag().first;
  std::cout << "node0 vertices " << node0_vertices << std::endl;
  for (int i(1); i < numberOfNodes - 1; i++) {
    const auto node_vertices = nodes[i]->getDagManager()->getNumVerticesInDag().first;
    std::cout << "node" << i << " vertices " << node_vertices << std::endl;
    if (node_vertices != node0_vertices) {
      dag_synced = false;
    }
  }
  // When last level have more than 1 DAG blocks, send a dummy transaction to converge DAG
  if (!dag_synced) {
    std::cout << "Send dummy trx" << std::endl;
    auto dummy_trx = std::make_shared<Transaction>(num_of_trxs++, 0, 2, TEST_TX_GAS_LIMIT, bytes(),
                                                   nodes[0]->getSecretKey(), nodes[0]->getAddress());
    // broadcast dummy transaction
    nodes[0]->getTransactionManager()->insertTransaction(dummy_trx);

    wait({60s, 100ms}, [&](auto& ctx) {
      // Check 4 nodes syncing
      for (int j = 1; j < numberOfNodes - 1; j++) {
        WAIT_EXPECT_EQ(ctx, nodes[j]->getDagManager()->getNumVerticesInDag().first,
                       nodes[0]->getDagManager()->getNumVerticesInDag().first);
        WAIT_EXPECT_EQ(ctx, nodes[j]->getPbftChain()->getPbftChainSizeExcludingEmptyPbftBlocks(),
                       nodes[0]->getPbftChain()->getPbftChainSizeExcludingEmptyPbftBlocks());
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
  }

  // Bootstrapping node5 join the network
  nodes.emplace_back(std::make_shared<FullNode>(node_cfgs[numberOfNodes - 1]));
  nodes.back()->start();
  EXPECT_TRUE(wait_connect(nodes));

  std::cout << "Waiting Sync for node5..." << std::endl;
  wait({60s, 100ms}, [&](auto& ctx) {
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
  std::cout << "node0 vertices " << node0_vertices << std::endl;
  for (int i(1); i < numberOfNodes; i++) {
    auto node_vertices = nodes[i]->getDagManager()->getNumVerticesInDag().first;
    std::cout << "node" << i << " vertices " << node_vertices << std::endl;
    if (node_vertices != node0_vertices) {
      dag_synced = false;
    }
  }
  // When last level have more than 1 DAG blocks, send a dummy transaction to converge DAG
  if (!dag_synced) {
    std::cout << "Send dummy trx" << std::endl;
    auto dummy_trx = std::make_shared<Transaction>(num_of_trxs++, 0, 2, TEST_TX_GAS_LIMIT, bytes(),
                                                   nodes[0]->getSecretKey(), nodes[0]->getAddress());
    // broadcast dummy transaction
    nodes[0]->getTransactionManager()->insertTransaction(dummy_trx);

    wait({60s, 100ms}, [&](auto& ctx) {
      // Check all 5 nodes DAG syncing
      for (int j = 1; j < numberOfNodes; j++) {
        WAIT_EXPECT_EQ(ctx, nodes[j]->getDagManager()->getNumVerticesInDag().first,
                       nodes[0]->getDagManager()->getNumVerticesInDag().first);
        WAIT_EXPECT_EQ(ctx, nodes[j]->getPbftChain()->getPbftChainSizeExcludingEmptyPbftBlocks(),
                       nodes[0]->getPbftChain()->getPbftChainSizeExcludingEmptyPbftBlocks());
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

TEST_F(NetworkTest, suspicious_packets) {
  network::tarcap::TaraxaPeer peer;
  // Verify that after 1000 reported suspicious packets true is returned
  for (int i = 0; i < 1000; i++) {
    EXPECT_FALSE(peer.reportSuspiciousPacket());
  }
  EXPECT_TRUE(peer.reportSuspiciousPacket());

  // This part of unit tests is commented out since it takes about one minute to actually test, run it if there are any
  // issues with this functionality

  /*thisThreadSleepForSeconds(60);
  for (int i = 0; i < 1000; i++) {
    EXPECT_FALSE(peer.reportSuspiciousPacket());
  }
  EXPECT_TRUE(peer.reportSuspiciousPacket());*/
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
