#include "network/network.hpp"

#include <gtest/gtest.h>

#include <atomic>
#include <iostream>
#include <vector>

#include "common/lazy.hpp"
#include "common/static_init.hpp"
#include "config/config.hpp"
#include "dag/dag.hpp"
#include "dag/dag_block_proposer.hpp"
#include "logger/logger.hpp"
#include "network/tarcap/packets_handlers/latest/dag_block_packet_handler.hpp"
#include "network/tarcap/packets_handlers/latest/get_dag_sync_packet_handler.hpp"
#include "network/tarcap/packets_handlers/latest/get_next_votes_bundle_packet_handler.hpp"
#include "network/tarcap/packets_handlers/latest/status_packet_handler.hpp"
#include "network/tarcap/packets_handlers/latest/transaction_packet_handler.hpp"
#include "network/tarcap/packets_handlers/latest/vote_packet_handler.hpp"
#include "network/tarcap/packets_handlers/latest/votes_bundle_packet_handler.hpp"
#include "pbft/pbft_manager.hpp"
#include "test_util/samples.hpp"
#include "test_util/test_util.hpp"

namespace taraxa::core_tests {

using dev::p2p::Host;
using vrf_wrapper::VrfSortitionBase;

const unsigned NUM_TRX = 10;
auto g_secret = Lazy([] {
  return dev::Secret("3800b2875669d9b2053c1aff9224ecfdc411423aac5b5a73d7a45ced1c3b9dcd",
                     dev::Secret::ConstructFromStringType::FromHex);
});
auto g_signed_trx_samples = Lazy([] { return samples::createSignedTrxSamples(0, NUM_TRX, g_secret); });

struct NetworkTest : public NodesTest {};

// Test verifies saving network to a file and restoring it from a file
// is successful. Once restored from the file it is able to reestablish
// connections even with boot nodes down
TEST_F(NetworkTest, save_network) {
  auto key2 = dev::KeyPair::create();
  auto key3 = dev::KeyPair::create();
  h256 genesis_hash;

  auto node_cfgs = make_node_cfgs(3, 0, 20);
  std::filesystem::remove_all(node_cfgs[0].net_file_path());
  std::filesystem::remove_all(node_cfgs[1].net_file_path());
  std::filesystem::remove_all(node_cfgs[2].net_file_path());

  {
    auto nodes = launch_nodes(node_cfgs);
    const auto& node1 = nodes[0];
    const auto& node2 = nodes[1];
    const auto& node3 = nodes[2];

    EXPECT_HAPPENS({120s, 100ms}, [&](auto& ctx) {
      WAIT_EXPECT_EQ(ctx, node1->getNetwork()->getPeerCount(), 2)
      WAIT_EXPECT_EQ(ctx, node2->getNetwork()->getPeerCount(), 2)
      WAIT_EXPECT_EQ(ctx, node3->getNetwork()->getPeerCount(), 2)
    });
  }

  auto nodes = launch_nodes({node_cfgs[0], node_cfgs[1]});
  const auto& node1 = nodes[0];
  const auto& node2 = nodes[1];

  EXPECT_HAPPENS({120s, 100ms}, [&](auto& ctx) {
    WAIT_EXPECT_EQ(ctx, node1->getNetwork()->getPeerCount(), 1)
    WAIT_EXPECT_EQ(ctx, node2->getNetwork()->getPeerCount(), 1)
  });
}

// Test creates two Network setup and verifies sending blocks between is successful
TEST_F(NetworkTest, transfer_lot_of_blocks) {
  auto node_cfgs = make_node_cfgs(2, 1, 20);
  auto nodes = launch_nodes(node_cfgs);
  const auto& node1 = nodes[0];
  const auto& node2 = nodes[1];

  // Stop PBFT manager
  node1->getPbftManager()->stop();
  node2->getPbftManager()->stop();

  const auto db1 = node1->getDB();
  const auto dag_mgr1 = node1->getDagManager();
  const auto dag_mgr2 = node2->getDagManager();
  const auto nw1 = node1->getNetwork();
  const auto nw2 = node2->getNetwork();

  auto trxs = samples::createSignedTrxSamples(0, 1500, g_secret);
  const auto estimation = node1->getTransactionManager()->estimateTransactionGas(trxs[0], {});

  // node1 add one valid block
  const auto proposal_level = 1;
  const auto proposal_period = *db1->getProposalPeriodForDagLevel(proposal_level);
  const auto period_block_hash = db1->getPeriodBlockHash(proposal_period);
  const auto sortition_params = dag_mgr1->sortitionParamsManager().getSortitionParams(proposal_period);
  vdf_sortition::VdfSortition vdf(sortition_params, node1->getVrfSecretKey(),
                                  VrfSortitionBase::makeVrfInput(proposal_level, period_block_hash), 1, 1);
  const auto dag_genesis = node1->getConfig().genesis.dag_genesis_block.getHash();
  dev::bytes vdf_msg = DagManager::getVdfMessage(dag_genesis, {trxs[0]});
  vdf.computeVdfSolution(sortition_params, vdf_msg, false);
  DagBlock blk(dag_genesis, proposal_level, {}, {trxs[0]->getHash()}, estimation, vdf, node1->getSecretKey());
  const auto block_hash = blk.getHash();
  dag_mgr1->addDagBlock(std::move(blk), {trxs[0]});
  std::vector<std::shared_ptr<DagBlock>> dag_blocks;

  {
    const auto proposal_period = *db1->getProposalPeriodForDagLevel(proposal_level + 1);
    const auto period_block_hash = db1->getPeriodBlockHash(proposal_period);
    const auto sortition_params = dag_mgr1->sortitionParamsManager().getSortitionParams(proposal_period);

    for (int i = 0; i < 100; ++i) {
      vdf_sortition::VdfSortition vdf(sortition_params, node1->getVrfSecretKey(),
                                      VrfSortitionBase::makeVrfInput(proposal_level + 1, period_block_hash), 1, 1);
      dev::bytes vdf_msg = DagManager::getVdfMessage(block_hash, {trxs[i]});
      vdf.computeVdfSolution(sortition_params, vdf_msg, false);
      DagBlock blk(block_hash, proposal_level + 1, {}, {trxs[i]->getHash()}, estimation, vdf, node1->getSecretKey());
      dag_blocks.emplace_back(std::make_shared<DagBlock>(blk));
    }
  }

  for (auto trx : trxs) {
    auto tx = trx;
    node1->getTransactionManager()->insertValidatedTransaction(std::move(tx), TransactionStatus::Verified);
  }
  for (size_t i = 0; i < dag_blocks.size(); i++) {
    if (dag_mgr1->verifyBlock(*dag_blocks[i]) == DagManager::VerifyBlockReturnType::Verified)
      dag_mgr1->addDagBlock(DagBlock(*dag_blocks[i]), {trxs[i]});
  }
  wait({1s, 200ms}, [&](auto& ctx) { WAIT_EXPECT_NE(ctx, dag_mgr1->getDagBlock(block_hash), nullptr) });
  const auto node1_period = node1->getPbftChain()->getPbftChainSize();
  const auto node2_period = node2->getPbftChain()->getPbftChainSize();
  std::cout << "node1 period " << node1_period << ", node2 period " << node2_period << std::endl;
  nw1->getSpecificHandler<network::tarcap::GetDagSyncPacketHandler>()->sendBlocks(
      nw2->getNodeId(), std::move(dag_blocks), std::move(trxs), node2_period, node1_period);
  std::cout << "Waiting Sync ..." << std::endl;
  wait({120s, 200ms}, [&](auto& ctx) { WAIT_EXPECT_NE(ctx, dag_mgr2->getDagBlock(block_hash), nullptr) });
}

// TODO: debug why the test take so long...
TEST_F(NetworkTest, propagate_block) {
  auto node_cfgs = make_node_cfgs(5, 1);
  auto nodes = launch_nodes(node_cfgs);
  const auto& node1 = nodes[0];

  // Stop PBFT manager
  for (auto& node : nodes) {
    node->getPbftManager()->stop();
  }

  const auto db1 = node1->getDB();
  const auto dag_mgr1 = node1->getDagManager();

  auto trxs = samples::createSignedTrxSamples(0, 1, g_secret);
  const auto estimation = node1->getTransactionManager()->estimateTransactionGas(trxs[0], {});

  // node1 add one valid block
  const auto proposal_level = 1;
  const auto proposal_period = *db1->getProposalPeriodForDagLevel(proposal_level);
  const auto period_block_hash = db1->getPeriodBlockHash(proposal_period);
  const auto sortition_params = dag_mgr1->sortitionParamsManager().getSortitionParams(proposal_period);
  vdf_sortition::VdfSortition vdf(sortition_params, node1->getVrfSecretKey(),
                                  VrfSortitionBase::makeVrfInput(proposal_level, period_block_hash), 1, 1);
  const auto dag_genesis = node1->getConfig().genesis.dag_genesis_block.getHash();
  dev::bytes vdf_msg = DagManager::getVdfMessage(dag_genesis, {trxs[0]});
  vdf.computeVdfSolution(sortition_params, vdf_msg, false);
  DagBlock blk(dag_genesis, proposal_level, {}, {trxs[0]->getHash()}, estimation, vdf, node1->getSecretKey());

  const auto block_hash = blk.getHash();

  // Add block gossip it to connected peers
  dag_mgr1->addDagBlock(std::move(blk), {trxs[0]});

  wait({1s, 200ms}, [&](auto& ctx) { WAIT_EXPECT_NE(ctx, dag_mgr1->getDagBlock(block_hash), nullptr) });

  std::cout << "Waiting Sync ..." << std::endl;
  wait({20s, 200ms}, [&](auto& ctx) {
    for (const auto& node : nodes) {
      const auto dag_mgr = node->getDagManager();
      WAIT_EXPECT_NE(ctx, dag_mgr->getDagBlock(block_hash), nullptr)
    }
  });
}

TEST_F(NetworkTest, DISABLED_update_peer_chainsize) {
  auto node_cfgs = make_node_cfgs(2, 1, 5);
  auto nodes = launch_nodes(node_cfgs);

  nodes[0]->getPbftManager()->stop();
  nodes[1]->getPbftManager()->stop();

  wait_connect(nodes);

  auto nw1 = nodes[0]->getNetwork();
  auto nw2 = nodes[1]->getNetwork();

  const auto& node1 = nodes[0];
  const auto& node1_pbft_chain = node1->getPbftChain();
  const PbftPeriod expected_chain_size = 10;

  // Artificially increase node1 chain_size, which is then sent through VotePacket
  for (auto i = node1_pbft_chain->getPbftChainSize(); i < expected_chain_size; i++) {
    node1_pbft_chain->updatePbftChain(kNullBlockHash, kNullBlockHash);
  }
  EXPECT_EQ(node1_pbft_chain->getPbftChainSize(), expected_chain_size);

  std::vector<vote_hash_t> reward_votes{};
  auto pbft_block = std::make_shared<PbftBlock>(blk_hash_t(1), kNullBlockHash, kNullBlockHash, kNullBlockHash,
                                                node1->getPbftManager()->getPbftPeriod(), node1->getAddress(),
                                                node1->getSecretKey(), std::move(reward_votes));
  auto vote = node1->getVoteManager()->generateVote(pbft_block->getBlockHash(), PbftVoteTypes::propose_vote,
                                                    pbft_block->getPeriod(),
                                                    node1->getPbftManager()->getPbftRound() + 1, value_proposal_state);

  auto node1_id = nw1->getNodeId();
  auto node2_id = nw2->getNodeId();

  EXPECT_NE(nw2->getPeer(node1_id)->pbft_chain_size_, expected_chain_size);
  nw1->getSpecificHandler<network::tarcap::VotePacketHandler>()->sendPbftVote(nw1->getPeer(node2_id), vote, pbft_block);
  EXPECT_HAPPENS({5s, 100ms},
                 [&](auto& ctx) { WAIT_EXPECT_EQ(ctx, nw2->getPeer(node1_id)->pbft_chain_size_, expected_chain_size) });
}

TEST_F(NetworkTest, malicious_peers) {
  FullNodeConfig conf;
  conf.network.peer_blacklist_timeout = 2;
  std::shared_ptr<dev::p2p::Host> host;
  EXPECT_EQ(conf.network.disable_peer_blacklist, false);
  network::tarcap::PeersState state1(host, conf);
  dev::p2p::NodeID id1(1);
  dev::p2p::NodeID id2(2);
  state1.set_peer_malicious(id1);
  EXPECT_EQ(state1.is_peer_malicious(id1), true);
  EXPECT_EQ(state1.is_peer_malicious(id2), false);

  conf.network.peer_blacklist_timeout = 0;
  network::tarcap::PeersState state2(host, conf);
  state2.set_peer_malicious(id1);
  EXPECT_EQ(state2.is_peer_malicious(id1), true);
  EXPECT_EQ(state2.is_peer_malicious(id2), false);

  conf.network.peer_blacklist_timeout = 2;
  conf.network.disable_peer_blacklist = true;
  network::tarcap::PeersState state3(host, conf);
  state1.set_peer_malicious(id1);
  EXPECT_EQ(state3.is_peer_malicious(id1), false);
  EXPECT_EQ(state3.is_peer_malicious(id2), false);

  conf.network.peer_blacklist_timeout = 0;
  conf.network.disable_peer_blacklist = true;
  network::tarcap::PeersState state4(host, conf);
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
  auto node_cfgs = make_node_cfgs(2, 1, 5);
  node_cfgs[0].genesis.pbft.gas_limit = TEST_BLOCK_GAS_LIMIT;
  node_cfgs[1].genesis.pbft.gas_limit = TEST_BLOCK_GAS_LIMIT;
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
  const auto node2_pbft_chain = nodes2[0]->getPbftChain();

  // verify that the large pbft block is synced
  EXPECT_HAPPENS({100s, 100ms}, [&](auto& ctx) {
    WAIT_EXPECT_EQ(ctx, node2_pbft_chain->getPbftChainSize(), node1_pbft_chain->getPbftChainSize())
  });

  auto pbft_blocks1 = nodes[0]->getDB()->getPbftBlock(non_empty_last_period);
  auto pbft_blocks2 = nodes2[0]->getDB()->getPbftBlock(non_empty_last_period);
  if (pbft_blocks1->rlp(true) != pbft_blocks2->rlp(true)) {
    std::cout << "PBFT block1 " << *pbft_blocks1 << std::endl;
    std::cout << "PBFT block2 " << *pbft_blocks2 << std::endl;
  }
  EXPECT_EQ(pbft_blocks1->rlp(true), pbft_blocks2->rlp(true));
}

// Test creates two Network setup and verifies sending transaction
// between is successful
TEST_F(NetworkTest, transfer_transaction) {
  auto node_cfgs = make_node_cfgs(2, 0, 20);
  auto nodes = launch_nodes(node_cfgs);
  const auto& node1 = nodes[0];
  const auto& node2 = nodes[1];

  // Stop PBFT manager
  node1->getPbftManager()->stop();
  node2->getPbftManager()->stop();

  const auto nw1 = node1->getNetwork();
  const auto nw2 = node2->getNetwork();

  auto nw1_nodeid = nw1->getNodeId();
  auto nw2_nodeid = nw2->getNodeId();

  const auto peer2 = nw1->getPeer(nw2_nodeid);
  const auto peer1 = nw2->getPeer(nw1_nodeid);
  EXPECT_NE(peer2, nullptr);
  EXPECT_NE(peer1, nullptr);

  SharedTransactions transactions;
  transactions.push_back(g_signed_trx_samples[0]);

  nw2->getSpecificHandler<network::tarcap::TransactionPacketHandler>()->sendTransactions(peer1,
                                                                                         std::move(transactions));
  const auto tx_mgr1 = node1->getTransactionManager();
  EXPECT_HAPPENS({2s, 200ms},
                 [&](auto& ctx) { WAIT_EXPECT_TRUE(ctx, tx_mgr1->getTransaction(g_signed_trx_samples[0]->getHash())) });
}

// Test creates one node with testnet network ID and one node with main ID and verifies that connection fails
TEST_F(NetworkTest, node_chain_id) {
  auto node_cfgs = make_node_cfgs(2);
  {
    auto node_cfgs_ = node_cfgs;
    node_cfgs_[0].genesis.chain_id = 1;
    node_cfgs_[1].genesis.chain_id = 1;
    auto nodes = launch_nodes(node_cfgs_);
  }
  // we need to cleanup datadirs because we saved previous genesis_hash in db. And it is different after chain_id
  // change
  CleanupDirs();
  {
    node_cfgs[0].genesis.chain_id = 1;
    node_cfgs[1].genesis.chain_id = 2;

    auto nodes = create_nodes(node_cfgs, true /*start*/);

    taraxa::thisThreadSleepForMilliSeconds(1000);
    EXPECT_EQ(nodes[0]->getNetwork()->getPeerCount(), 0);
    EXPECT_EQ(nodes[1]->getNetwork()->getPeerCount(), 0);
  }
}

// Test creates a DAG on one node and verifies that the second node syncs with it and that the resulting DAG on the
// other end is the same
TEST_F(NetworkTest, node_sync) {
  auto node_cfgs = make_node_cfgs(2, 1, 5);
  auto node1 = create_nodes({node_cfgs[0]}, true /*start*/).front();
  // Stop PBFT manager
  node1->getPbftManager()->stop();

  // Allow node to start up
  taraxa::thisThreadSleepForMilliSeconds(1000);

  std::vector<std::pair<DagBlock, std::shared_ptr<Transaction>>> blks;
  // Generate DAG blocks
  const auto dag_genesis = node1->getConfig().genesis.dag_genesis_block.getHash();
  const auto sk = node1->getSecretKey();
  const auto vrf_sk = node1->getVrfSecretKey();
  const auto estimation = node1->getTransactionManager()->estimateTransactionGas(g_signed_trx_samples[0], {});
  SortitionConfig vdf_config(node_cfgs[0].genesis.sortition);

  auto propose_level = 1;
  const auto period_block_hash = node1->getDB()->getPeriodBlockHash(propose_level);
  vdf_sortition::VdfSortition vdf1(vdf_config, vrf_sk, VrfSortitionBase::makeVrfInput(propose_level, period_block_hash),
                                   1, 1);

  dev::bytes vdf_msg1 = DagManager::getVdfMessage(dag_genesis, {g_signed_trx_samples[1]});
  vdf1.computeVdfSolution(vdf_config, vdf_msg1, false);
  DagBlock blk1(dag_genesis, propose_level, {}, {g_signed_trx_samples[1]->getHash()}, estimation, vdf1, sk);

  propose_level = 2;
  vdf_sortition::VdfSortition vdf2(vdf_config, vrf_sk, VrfSortitionBase::makeVrfInput(propose_level, period_block_hash),
                                   1, 1);
  dev::bytes vdf_msg2 = DagManager::getVdfMessage(blk1.getHash(), {g_signed_trx_samples[2]});
  vdf2.computeVdfSolution(vdf_config, vdf_msg2, false);
  DagBlock blk2(blk1.getHash(), propose_level, {}, {g_signed_trx_samples[2]->getHash()}, estimation, vdf2, sk);

  propose_level = 3;
  vdf_sortition::VdfSortition vdf3(vdf_config, vrf_sk, VrfSortitionBase::makeVrfInput(propose_level, period_block_hash),
                                   1, 1);
  dev::bytes vdf_msg3 = DagManager::getVdfMessage(blk2.getHash(), {g_signed_trx_samples[3]});
  vdf3.computeVdfSolution(vdf_config, vdf_msg3, false);
  DagBlock blk3(blk2.getHash(), propose_level, {}, {g_signed_trx_samples[3]->getHash()}, estimation, vdf3, sk);

  propose_level = 4;
  vdf_sortition::VdfSortition vdf4(vdf_config, vrf_sk, VrfSortitionBase::makeVrfInput(propose_level, period_block_hash),
                                   1, 1);
  dev::bytes vdf_msg4 = DagManager::getVdfMessage(blk3.getHash(), {g_signed_trx_samples[4]});
  vdf4.computeVdfSolution(vdf_config, vdf_msg4, false);
  DagBlock blk4(blk3.getHash(), propose_level, {}, {g_signed_trx_samples[4]->getHash()}, estimation, vdf4, sk);

  propose_level = 5;
  vdf_sortition::VdfSortition vdf5(vdf_config, vrf_sk, VrfSortitionBase::makeVrfInput(propose_level, period_block_hash),
                                   1, 1);
  dev::bytes vdf_msg5 = DagManager::getVdfMessage(blk4.getHash(), {g_signed_trx_samples[5]});
  vdf5.computeVdfSolution(vdf_config, vdf_msg5, false);
  DagBlock blk5(blk4.getHash(), propose_level, {}, {g_signed_trx_samples[5]->getHash()}, estimation, vdf5, sk);

  propose_level = 6;
  vdf_sortition::VdfSortition vdf6(vdf_config, vrf_sk, VrfSortitionBase::makeVrfInput(propose_level, period_block_hash),
                                   1, 1);
  dev::bytes vdf_msg6 = DagManager::getVdfMessage(blk5.getHash(), {g_signed_trx_samples[6]});
  vdf6.computeVdfSolution(vdf_config, vdf_msg6, false);
  DagBlock blk6(blk5.getHash(), propose_level, {blk4.getHash(), blk3.getHash()}, {g_signed_trx_samples[6]->getHash()},
                estimation, vdf6, sk);

  blks.push_back(std::make_pair(blk1, g_signed_trx_samples[1]));
  blks.push_back(std::make_pair(blk2, g_signed_trx_samples[2]));
  blks.push_back(std::make_pair(blk3, g_signed_trx_samples[3]));
  blks.push_back(std::make_pair(blk4, g_signed_trx_samples[4]));
  blks.push_back(std::make_pair(blk5, g_signed_trx_samples[5]));
  blks.push_back(std::make_pair(blk6, g_signed_trx_samples[6]));

  for (size_t i = 0; i < blks.size(); ++i) {
    node1->getTransactionManager()->insertValidatedTransaction(std::move(blks[i].second), TransactionStatus::Verified);
    EXPECT_EQ(node1->getDagManager()->verifyBlock(blks[i].first), DagManager::VerifyBlockReturnType::Verified);
    node1->getDagManager()->addDagBlock(std::move(blks[i].first));
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
  auto node_cfgs = make_node_cfgs(2, 1, 20);
  auto node1 = create_nodes({node_cfgs[0]}, true /*start*/).front();

  // Stop PBFT manager and executor for syncing test
  node1->getPbftManager()->stop();

  auto db1 = node1->getDB();
  auto pbft_chain1 = node1->getPbftChain();

  auto dag_genesis = node1->getConfig().genesis.dag_genesis_block.getHash();
  auto sk = node1->getSecretKey();
  auto vrf_sk = node1->getVrfSecretKey();
  SortitionConfig vdf_config(node_cfgs[0].genesis.sortition);
  auto batch = db1->createWriteBatch();

  // generate first PBFT block sample
  blk_hash_t prev_block_hash(0);
  PbftPeriod period = 1;
  addr_t beneficiary(987);

  level_t level = 1;
  vdf_sortition::VdfSortition vdf1(vdf_config, vrf_sk, getRlpBytes(level), 1, 1);
  dev::bytes vdf_msg1 = DagManager::getVdfMessage(dag_genesis, {g_signed_trx_samples[0], g_signed_trx_samples[1]});
  vdf1.computeVdfSolution(vdf_config, vdf_msg1, false);
  DagBlock blk1(dag_genesis, 1, {}, {g_signed_trx_samples[0]->getHash(), g_signed_trx_samples[1]->getHash()}, 0, vdf1,
                sk);
  node1->getTransactionManager()->insertValidatedTransaction(std::shared_ptr<Transaction>(g_signed_trx_samples[0]),
                                                             TransactionStatus::Verified);
  node1->getTransactionManager()->insertValidatedTransaction(std::shared_ptr<Transaction>(g_signed_trx_samples[1]),
                                                             TransactionStatus::Verified);
  node1->getDagManager()->verifyBlock(DagBlock(blk1));
  node1->getDagManager()->addDagBlock(DagBlock(blk1));

  dev::RLPStream order_stream(1);
  order_stream.appendList(1);
  order_stream << blk1.getHash();

  PbftBlock pbft_block1(prev_block_hash, blk1.getHash(), dev::sha3(order_stream.out()), kNullBlockHash, period,
                        beneficiary, node1->getSecretKey(), {});
  std::vector<std::shared_ptr<Vote>> votes_for_pbft_blk1;
  votes_for_pbft_blk1.emplace_back(
      node1->getVoteManager()->generateVote(pbft_block1.getBlockHash(), PbftVoteTypes::cert_vote, 1, 1, 3));
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
  pbft_chain1->updatePbftChain(pbft_block1.getBlockHash(), pbft_block1.getPivotDagBlockHash());
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
  vdf_sortition::VdfSortition vdf2(vdf_config, vrf_sk, getRlpBytes(level), 1, 1);
  dev::bytes vdf_msg2 = DagManager::getVdfMessage(blk1.getHash(), {g_signed_trx_samples[2], g_signed_trx_samples[3]});
  vdf2.computeVdfSolution(vdf_config, vdf_msg2, false);
  DagBlock blk2(blk1.getHash(), 2, {}, {g_signed_trx_samples[2]->getHash(), g_signed_trx_samples[3]->getHash()}, 0,
                vdf2, sk);
  node1->getTransactionManager()->insertValidatedTransaction(std::shared_ptr(g_signed_trx_samples[2]),
                                                             TransactionStatus::Verified);
  node1->getTransactionManager()->insertValidatedTransaction(std::shared_ptr(g_signed_trx_samples[3]),
                                                             TransactionStatus::Verified);
  node1->getDagManager()->verifyBlock(DagBlock(blk2));
  node1->getDagManager()->addDagBlock(DagBlock(blk2));

  batch = db1->createWriteBatch();
  period = 2;
  beneficiary = addr_t(654);
  dev::RLPStream order_stream2(1);
  order_stream2.appendList(1);
  order_stream2 << blk2.getHash();
  PbftBlock pbft_block2(prev_block_hash, blk2.getHash(), dev::sha3(order_stream2.out()), kNullBlockHash, period,
                        beneficiary, node1->getSecretKey(), {});
  std::vector<std::shared_ptr<Vote>> votes_for_pbft_blk2;
  votes_for_pbft_blk2.emplace_back(
      node1->getVoteManager()->generateVoteWithWeight(pbft_block2.getBlockHash(), PbftVoteTypes::cert_vote, 2, 1, 3));
  std::cout << "Generate 1 vote for second PBFT block" << std::endl;
  // node1 put block2 into pbft chain and store into DB
  // Add cert votes in DB
  // Add PBFT block in DB

  std::cout << "B1 " << pbft_block2.getBlockHash() << std::endl;

  PeriodData period_data2(std::make_shared<PbftBlock>(pbft_block2), votes_for_pbft_blk1);
  period_data2.dag_blocks.push_back(blk2);
  period_data2.transactions.push_back(g_signed_trx_samples[2]);
  period_data2.transactions.push_back(g_signed_trx_samples[3]);

  db1->savePeriodData(period_data2, batch);
  node1->getVoteManager()->addVerifiedVote(votes_for_pbft_blk2[0]);
  db1->replaceTwoTPlusOneVotesToBatch(TwoTPlusOneVotedBlockType::CertVotedBlock, votes_for_pbft_blk2, batch);
  node1->getVoteManager()->resetRewardVotes(2, 1, 3, pbft_block2.getBlockHash(), batch);

  // Update pbft chain
  pbft_chain1->updatePbftChain(pbft_block2.getBlockHash(), pbft_block2.getPivotDagBlockHash());
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
  EXPECT_TRUE(wait_connect({node1, node2}));

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
  auto node_cfgs = make_node_cfgs(2, 1, 20);
  auto node1 = create_nodes({node_cfgs[0]}, true /*start*/).front();

  // Stop PBFT manager and executor for syncing test
  node1->getPbftManager()->stop();

  auto db1 = node1->getDB();
  auto pbft_chain1 = node1->getPbftChain();

  auto dag_genesis = node1->getConfig().genesis.dag_genesis_block.getHash();
  auto sk = node1->getSecretKey();
  auto vrf_sk = node1->getVrfSecretKey();
  SortitionConfig vdf_config(node_cfgs[0].genesis.sortition);
  auto batch = db1->createWriteBatch();

  // generate first PBFT block sample
  blk_hash_t prev_block_hash(0);
  PbftPeriod period = 1;
  addr_t beneficiary(876);
  level_t level = 1;
  vdf_sortition::VdfSortition vdf1(vdf_config, vrf_sk, getRlpBytes(level), 1, 1);
  dev::bytes vdf_msg1 = DagManager::getVdfMessage(dag_genesis, {g_signed_trx_samples[0], g_signed_trx_samples[1]});
  vdf1.computeVdfSolution(vdf_config, vdf_msg1, false);
  DagBlock blk1(dag_genesis, 1, {}, {g_signed_trx_samples[0]->getHash(), g_signed_trx_samples[1]->getHash()}, 0, vdf1,
                sk);
  node1->getTransactionManager()->insertValidatedTransaction(std::shared_ptr(g_signed_trx_samples[0]),
                                                             TransactionStatus::Verified);
  node1->getTransactionManager()->insertValidatedTransaction(std::shared_ptr(g_signed_trx_samples[1]),
                                                             TransactionStatus::Verified);
  node1->getDagManager()->verifyBlock(DagBlock(blk1));
  node1->getDagManager()->addDagBlock(DagBlock(blk1));

  dev::RLPStream order_stream(1);
  order_stream.appendList(1);
  order_stream << blk1.getHash();

  PbftBlock pbft_block1(prev_block_hash, blk1.getHash(), dev::sha3(order_stream.out()), kNullBlockHash, period,
                        beneficiary, node1->getSecretKey(), {});
  const auto pbft_block1_cert_vote = node1->getVoteManager()->generateVote(
      pbft_block1.getBlockHash(), PbftVoteTypes::cert_vote, pbft_block1.getPeriod(), 1, 3);
  pbft_block1_cert_vote->calculateWeight(1, 1, 1);
  node1->getVoteManager()->addVerifiedVote(pbft_block1_cert_vote);

  // Add PBFT block in DB
  PeriodData period_data1(std::make_shared<PbftBlock>(pbft_block1), {});
  period_data1.dag_blocks.push_back(blk1);
  period_data1.transactions.push_back(g_signed_trx_samples[0]);
  period_data1.transactions.push_back(g_signed_trx_samples[1]);

  db1->savePeriodData(period_data1, batch);
  // Update pbft chain
  pbft_chain1->updatePbftChain(pbft_block1.getBlockHash(), pbft_block1.getPivotDagBlockHash());
  // Update PBFT chain head block
  blk_hash_t pbft_chain_head_hash = pbft_chain1->getHeadHash();
  std::string pbft_chain_head_str = pbft_chain1->getJsonStr();
  db1->addPbftHeadToBatch(pbft_chain_head_hash, pbft_chain_head_str, batch);
  db1->commitWriteBatch(batch);
  PbftPeriod expect_pbft_chain_size = 1;
  EXPECT_EQ(node1->getPbftChain()->getPbftChainSize(), expect_pbft_chain_size);

  // generate second PBFT block sample
  prev_block_hash = pbft_block1.getBlockHash();
  level = 2;
  vdf_sortition::VdfSortition vdf2(vdf_config, vrf_sk, getRlpBytes(level), 1, 1);
  dev::bytes vdf_msg2 = DagManager::getVdfMessage(blk1.getHash(), {g_signed_trx_samples[2], g_signed_trx_samples[3]});
  vdf2.computeVdfSolution(vdf_config, vdf_msg2, false);
  DagBlock blk2(blk1.getHash(), 2, {}, {g_signed_trx_samples[2]->getHash(), g_signed_trx_samples[3]->getHash()}, 0,
                vdf2, sk);
  node1->getTransactionManager()->insertValidatedTransaction(std::shared_ptr(g_signed_trx_samples[2]),
                                                             TransactionStatus::Verified);
  node1->getTransactionManager()->insertValidatedTransaction(std::shared_ptr(g_signed_trx_samples[3]),
                                                             TransactionStatus::Verified);
  node1->getDagManager()->verifyBlock(DagBlock(blk2));
  node1->getDagManager()->addDagBlock(DagBlock(blk2));

  batch = db1->createWriteBatch();
  period = 2;
  beneficiary = addr_t(654);

  dev::RLPStream order_stream2(1);
  order_stream2.appendList(1);
  order_stream2 << blk2.getHash();

  PbftBlock pbft_block2(prev_block_hash, blk2.getHash(), dev::sha3(order_stream2.out()), kNullBlockHash, period,
                        beneficiary, node1->getSecretKey(), {});
  const auto pbft_block2_cert_vote = node1->getVoteManager()->generateVote(
      pbft_block2.getBlockHash(), PbftVoteTypes::cert_vote, pbft_block2.getPeriod(), 1, 3);
  pbft_block2_cert_vote->calculateWeight(1, 1, 1);
  node1->getVoteManager()->addVerifiedVote(pbft_block2_cert_vote);

  PeriodData period_data2(std::make_shared<PbftBlock>(pbft_block2), {pbft_block1_cert_vote});
  period_data2.dag_blocks.push_back(blk2);
  period_data2.transactions.push_back(g_signed_trx_samples[2]);
  period_data2.transactions.push_back(g_signed_trx_samples[3]);
  db1->savePeriodData(period_data2, batch);

  // Update pbft chain
  pbft_chain1->updatePbftChain(pbft_block2.getBlockHash(), pbft_block2.getPivotDagBlockHash());
  // Update PBFT chain head block
  pbft_chain_head_hash = pbft_chain1->getHeadHash();
  pbft_chain_head_str = pbft_chain1->getJsonStr();
  db1->addPbftHeadToBatch(pbft_chain_head_hash, pbft_chain_head_str, batch);

  node1->getVoteManager()->resetRewardVotes(pbft_block2.getPeriod(), 1, 3, pbft_block2.getBlockHash(), batch);

  db1->commitWriteBatch(batch);

  expect_pbft_chain_size = 2;
  EXPECT_EQ(node1->getPbftChain()->getPbftChainSize(), expect_pbft_chain_size);

  auto node2 = create_nodes({node_cfgs[1]}, true /*start*/).front();
  std::cout << "Waiting Sync for max 1 minutes..." << std::endl;
  for (int i = 0; i < 600; i++) {
    if (node2->getPbftManager()->pbftSyncingPeriod() >= expect_pbft_chain_size) {
      break;
    }
    taraxa::thisThreadSleepForMilliSeconds(100);
  }
  EXPECT_EQ(node2->getPbftManager()->pbftSyncingPeriod(), expect_pbft_chain_size);
}

// Test PBFT next votes sycning when node is behind of PBFT round with peer
TEST_F(NetworkTest, pbft_next_votes_sync_in_behind_round) {
  auto node_cfgs = make_node_cfgs(2, 1, 20);

  auto node1 = create_nodes({node_cfgs[0]}, true /*start*/).front();
  auto pbft_mgr1 = node1->getPbftManager();
  auto vote_mgr1 = node1->getVoteManager();
  pbft_mgr1->stop();  // Stop PBFT manager, that will place vote

  auto node2 = create_nodes({node_cfgs[1]}, true /*start*/).front();
  auto pbft_mgr2 = node2->getPbftManager();
  pbft_mgr2->stop();  // Stop PBFT manager, that will place vote

  clearAllVotes({node1, node2});

  // Generate 3 next votes
  PbftVoteTypes type = PbftVoteTypes::next_vote;
  PbftPeriod period = 1;
  PbftRound round = 1;
  pbft_mgr1->setPbftRound(round + 1);

  // Gen 2 votes in second finish step(5) for null block hash as well as some specific hash
  auto null_next_vote = vote_mgr1->generateVote(kNullBlockHash, type, period, round, 5);
  null_next_vote->calculateWeight(1, 1, 1);
  vote_mgr1->addVerifiedVote(null_next_vote);

  auto next_vote = vote_mgr1->generateVote(blk_hash_t(1), type, period, round, 5);
  next_vote->calculateWeight(1, 1, 1);
  vote_mgr1->addVerifiedVote(next_vote);

  const size_t votes_count = 2;
  EXPECT_EQ(vote_mgr1->getVerifiedVotesSize(), votes_count);

  // Make sure node2 PBFT round is less than node1
  pbft_mgr2->setPbftRound(round);

  // Wait node1 and node2 connect to each other
  wait_connect({node1, node2});

  // Node2 wait for getting votes from node1 by sending status packet
  EXPECT_HAPPENS({10s, 500ms},
                 [&](auto& ctx) { WAIT_EXPECT_EQ(ctx, node2->getVoteManager()->getVerifiedVotesSize(), votes_count) });
}

// Test PBFT next votes syncing when nodes stay at same PBFT round
TEST_F(NetworkTest, pbft_next_votes_sync_in_same_round) {
  const auto node_cfgs = make_node_cfgs(2, 1, 20);

  const auto node1 = std::make_shared<FullNode>(node_cfgs[0]);
  node1->start();
  const auto node1_pbft_mgr = node1->getPbftManager();
  const auto node1_vote_mgr = node1->getVoteManager();
  node1_pbft_mgr->stop();

  const auto node2 = std::make_shared<FullNode>(node_cfgs[1]);
  node2->start();
  const auto node2_pbft_mgr = node2->getPbftManager();
  const auto node2_vote_mgr = node2->getVoteManager();
  node2_pbft_mgr->stop();

  EXPECT_TRUE(wait_connect({node1, node2}));

  // Clear all votes
  clearAllVotes({node1, node2});

  auto node1_pbft_2t_plus_1 =
      node1_vote_mgr->getPbftTwoTPlusOne(node1->getPbftChain()->getPbftChainSize(), PbftVoteTypes::next_vote).value();
  EXPECT_EQ(node1_pbft_2t_plus_1, 1);
  auto node2_pbft_2t_plus_1 =
      node2_vote_mgr->getPbftTwoTPlusOne(node2->getPbftChain()->getPbftChainSize(), PbftVoteTypes::next_vote).value();
  EXPECT_EQ(node2_pbft_2t_plus_1, 1);

  // Node1 generate 1 next vote voted at kNullBlockHash
  PbftVoteTypes type = PbftVoteTypes::next_vote;
  PbftPeriod period = 1;
  PbftRound round = 1;
  PbftStep step = 5;

  // Add 1 vote to node1 verified votes
  EXPECT_EQ(node1_vote_mgr->getVerifiedVotesSize(), 0);
  node1_vote_mgr->addVerifiedVote(genDummyVote(type, period, round, step, kNullBlockHash, node1_vote_mgr));
  node1_vote_mgr->addVerifiedVote(genDummyVote(type, period, round, step, blk_hash_t(1), node1_vote_mgr));
  EXPECT_EQ(node1_vote_mgr->getVerifiedVotesSize(), 2);

  // Add 1 vote to node2 verified votes
  EXPECT_EQ(node2_vote_mgr->getVerifiedVotesSize(), 0);
  node2_vote_mgr->addVerifiedVote(genDummyVote(type, period, round, step, blk_hash_t(1), node2_vote_mgr));
  EXPECT_EQ(node2_vote_mgr->getVerifiedVotesSize(), 1);

  // Set both node1 and node2 pbft manager round to 2
  node1->getPbftManager()->setPbftRound(2);
  node2->getPbftManager()->setPbftRound(2);

  // Node 1 broadcast his votes
  node1_pbft_mgr->testBroadcatVotesFunctionality();
  // Node 2 should receive votes from node 1, node 1 has its own 2 votes
  EXPECT_EQ(node1_vote_mgr->getVerifiedVotesSize(), 2);
  EXPECT_HAPPENS({5s, 100ms}, [&](auto& ctx) { WAIT_EXPECT_EQ(ctx, node2_vote_mgr->getVerifiedVotesSize(), 3) });
}

// Test creates a DAG on one node and verifies
// that the second node syncs with it and that the resulting
// DAG on the other end is the same
// Unlike the previous tests, this DAG contains blocks with transactions
// and verifies that the sync containing transactions is successful
TEST_F(NetworkTest, node_sync_with_transactions) {
  auto node_cfgs = make_node_cfgs(2, 1, 5);
  auto node1 = create_nodes({node_cfgs[0]}, true /*start*/).front();

  std::vector<DagBlock> blks;
  // Generate DAG blocks
  const auto dag_genesis = node1->getConfig().genesis.dag_genesis_block.getHash();
  const auto sk = node1->getSecretKey();
  const auto vrf_sk = node1->getVrfSecretKey();
  const auto estimation = node1->getTransactionManager()->estimateTransactionGas(g_signed_trx_samples[0], {});

  SortitionConfig vdf_config(node_cfgs[0].genesis.sortition);
  auto propose_level = 1;
  const auto period_block_hash = node1->getDB()->getPeriodBlockHash(propose_level);
  vdf_sortition::VdfSortition vdf1(vdf_config, vrf_sk, VrfSortitionBase::makeVrfInput(propose_level, period_block_hash),
                                   1, 1);
  dev::RLPStream s;
  dev::bytes vdf_msg1 = DagManager::getVdfMessage(dag_genesis, {g_signed_trx_samples[0], g_signed_trx_samples[1]});
  vdf1.computeVdfSolution(vdf_config, vdf_msg1, false);
  DagBlock blk1(dag_genesis, propose_level, {},
                {g_signed_trx_samples[0]->getHash(), g_signed_trx_samples[1]->getHash()}, 2 * estimation, vdf1, sk);
  std::vector<std::pair<std::shared_ptr<Transaction>, TransactionStatus>> tr1{
      {g_signed_trx_samples[0], TransactionStatus::Verified}, {g_signed_trx_samples[1], TransactionStatus::Verified}};

  propose_level = 2;
  vdf_sortition::VdfSortition vdf2(vdf_config, vrf_sk, VrfSortitionBase::makeVrfInput(propose_level, period_block_hash),
                                   1, 1);
  dev::bytes vdf_msg2 = DagManager::getVdfMessage(blk1.getHash(), {g_signed_trx_samples[2]});
  vdf2.computeVdfSolution(vdf_config, vdf_msg2, false);
  DagBlock blk2(blk1.getHash(), propose_level, {}, {g_signed_trx_samples[2]->getHash()}, estimation, vdf2, sk);
  std::vector<std::pair<std::shared_ptr<Transaction>, TransactionStatus>> tr2{
      {g_signed_trx_samples[2], TransactionStatus::Verified}};

  propose_level = 3;
  vdf_sortition::VdfSortition vdf3(vdf_config, vrf_sk, VrfSortitionBase::makeVrfInput(propose_level, period_block_hash),
                                   1, 1);
  dev::bytes vdf_msg3 = DagManager::getVdfMessage(blk2.getHash(), {g_signed_trx_samples[3]});
  vdf3.computeVdfSolution(vdf_config, vdf_msg3, false);
  DagBlock blk3(blk2.getHash(), propose_level, {}, {g_signed_trx_samples[3]->getHash()}, estimation, vdf3, sk);
  std::vector<std::pair<std::shared_ptr<Transaction>, TransactionStatus>> tr3{
      {g_signed_trx_samples[3], TransactionStatus::Verified}};

  propose_level = 4;
  vdf_sortition::VdfSortition vdf4(vdf_config, vrf_sk, VrfSortitionBase::makeVrfInput(propose_level, period_block_hash),
                                   1, 1);
  dev::bytes vdf_msg4 = DagManager::getVdfMessage(blk3.getHash(), {g_signed_trx_samples[4]});
  vdf4.computeVdfSolution(vdf_config, vdf_msg4, false);
  DagBlock blk4(blk3.getHash(), propose_level, {}, {g_signed_trx_samples[4]->getHash()}, estimation, vdf4, sk);
  std::vector<std::pair<std::shared_ptr<Transaction>, TransactionStatus>> tr4{
      {g_signed_trx_samples[4], TransactionStatus::Verified}};

  propose_level = 5;
  vdf_sortition::VdfSortition vdf5(vdf_config, vrf_sk, VrfSortitionBase::makeVrfInput(propose_level, period_block_hash),
                                   1, 1);
  dev::bytes vdf_msg5 = DagManager::getVdfMessage(blk4.getHash(), {g_signed_trx_samples[5], g_signed_trx_samples[6],
                                                                   g_signed_trx_samples[7], g_signed_trx_samples[8]});
  vdf5.computeVdfSolution(vdf_config, vdf_msg5, false);
  DagBlock blk5(blk4.getHash(), propose_level, {},
                {g_signed_trx_samples[5]->getHash(), g_signed_trx_samples[6]->getHash(),
                 g_signed_trx_samples[7]->getHash(), g_signed_trx_samples[8]->getHash()},
                4 * estimation, vdf5, sk);
  std::vector<std::pair<std::shared_ptr<Transaction>, TransactionStatus>> tr5{
      {g_signed_trx_samples[5], TransactionStatus::Verified},
      {g_signed_trx_samples[6], TransactionStatus::Verified},
      {g_signed_trx_samples[7], TransactionStatus::Verified},
      {g_signed_trx_samples[8], TransactionStatus::Verified}};

  propose_level = 6;
  vdf_sortition::VdfSortition vdf6(vdf_config, vrf_sk, VrfSortitionBase::makeVrfInput(propose_level, period_block_hash),
                                   1, 1);
  dev::bytes vdf_msg6 = DagManager::getVdfMessage(blk5.getHash(), {g_signed_trx_samples[9]});
  vdf6.computeVdfSolution(vdf_config, vdf_msg6, false);
  DagBlock blk6(blk5.getHash(), propose_level, {blk4.getHash(), blk3.getHash()}, {g_signed_trx_samples[9]->getHash()},
                estimation, vdf6, sk);
  std::vector<std::pair<std::shared_ptr<Transaction>, TransactionStatus>> tr6{
      {g_signed_trx_samples[9], TransactionStatus::Verified}};

  node1->getTransactionManager()->insertValidatedTransaction(std::shared_ptr(g_signed_trx_samples[0]),
                                                             TransactionStatus::Verified);
  node1->getTransactionManager()->insertValidatedTransaction(std::shared_ptr(g_signed_trx_samples[1]),
                                                             TransactionStatus::Verified);
  EXPECT_EQ(node1->getDagManager()->verifyBlock(std::move(blk1)), DagManager::VerifyBlockReturnType::Verified);
  node1->getDagManager()->addDagBlock(DagBlock(blk1));
  node1->getTransactionManager()->insertValidatedTransaction(std::shared_ptr(g_signed_trx_samples[2]),
                                                             TransactionStatus::Verified);
  EXPECT_EQ(node1->getDagManager()->verifyBlock(std::move(blk2)), DagManager::VerifyBlockReturnType::Verified);
  node1->getDagManager()->addDagBlock(DagBlock(blk2));
  node1->getTransactionManager()->insertValidatedTransaction(std::shared_ptr(g_signed_trx_samples[3]),
                                                             TransactionStatus::Verified);
  EXPECT_EQ(node1->getDagManager()->verifyBlock(std::move(blk3)), DagManager::VerifyBlockReturnType::Verified);
  node1->getDagManager()->addDagBlock(DagBlock(blk3));
  node1->getTransactionManager()->insertValidatedTransaction(std::shared_ptr(g_signed_trx_samples[4]),
                                                             TransactionStatus::Verified);
  EXPECT_EQ(node1->getDagManager()->verifyBlock(std::move(blk4)), DagManager::VerifyBlockReturnType::Verified);
  node1->getDagManager()->addDagBlock(DagBlock(blk4));
  node1->getTransactionManager()->insertValidatedTransaction(std::shared_ptr(g_signed_trx_samples[5]),
                                                             TransactionStatus::Verified);
  node1->getTransactionManager()->insertValidatedTransaction(std::shared_ptr(g_signed_trx_samples[6]),
                                                             TransactionStatus::Verified);
  node1->getTransactionManager()->insertValidatedTransaction(std::shared_ptr(g_signed_trx_samples[7]),
                                                             TransactionStatus::Verified);
  node1->getTransactionManager()->insertValidatedTransaction(std::shared_ptr(g_signed_trx_samples[8]),
                                                             TransactionStatus::Verified);
  EXPECT_EQ(node1->getDagManager()->verifyBlock(std::move(blk5)), DagManager::VerifyBlockReturnType::Verified);
  node1->getDagManager()->addDagBlock(DagBlock(blk5));
  node1->getTransactionManager()->insertValidatedTransaction(std::shared_ptr(g_signed_trx_samples[9]),
                                                             TransactionStatus::Verified);
  EXPECT_EQ(node1->getDagManager()->verifyBlock(std::move(blk6)), DagManager::VerifyBlockReturnType::Verified);
  node1->getDagManager()->addDagBlock(DagBlock(blk6));
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
  auto node_cfgs = make_node_cfgs(2, 1, 5);
  auto node1 = create_nodes({node_cfgs[0]}, true /*start*/).front();

  std::vector<DagBlock> blks;
  // Generate DAG blocks
  const auto dag_genesis = node1->getConfig().genesis.dag_genesis_block.getHash();
  const auto sk = node1->getSecretKey();
  const auto vrf_sk = node1->getVrfSecretKey();
  const SortitionConfig vdf_config(node_cfgs[0].genesis.sortition);
  const auto transactions = samples::createSignedTrxSamples(0, 25, sk);
  const auto estimation = node1->getTransactionManager()->estimateTransactionGas(transactions[0], {});
  // DAG block1
  auto propose_level = 1;
  const auto period_block_hash = node1->getDB()->getPeriodBlockHash(propose_level);
  vdf_sortition::VdfSortition vdf1(vdf_config, vrf_sk, VrfSortitionBase::makeVrfInput(propose_level, period_block_hash),
                                   1, 1);
  dev::bytes vdf_msg = DagManager::getVdfMessage(dag_genesis, {transactions[0], transactions[1]});
  vdf1.computeVdfSolution(vdf_config, vdf_msg, false);
  DagBlock blk1(dag_genesis, propose_level, {}, {transactions[0]->getHash(), transactions[1]->getHash()},
                2 * estimation, vdf1, sk);
  SharedTransactions tr1({transactions[0], transactions[1]});
  // DAG block2
  propose_level = 1;
  vdf_sortition::VdfSortition vdf2(vdf_config, vrf_sk, VrfSortitionBase::makeVrfInput(propose_level, period_block_hash),
                                   1, 1);
  vdf_msg = DagManager::getVdfMessage(dag_genesis, {transactions[2], transactions[3]});
  vdf2.computeVdfSolution(vdf_config, vdf_msg, false);
  DagBlock blk2(dag_genesis, propose_level, {}, {transactions[2]->getHash(), transactions[3]->getHash()},
                2 * estimation, vdf2, sk);
  SharedTransactions tr2({transactions[2], transactions[3]});
  // DAG block3
  propose_level = 2;
  vdf_sortition::VdfSortition vdf3(vdf_config, vrf_sk, VrfSortitionBase::makeVrfInput(propose_level, period_block_hash),
                                   1, 1);
  vdf_msg = DagManager::getVdfMessage(blk1.getHash(), {transactions[4], transactions[5]});
  vdf3.computeVdfSolution(vdf_config, vdf_msg, false);
  DagBlock blk3(blk1.getHash(), propose_level, {}, {transactions[4]->getHash(), transactions[5]->getHash()},
                2 * estimation, vdf3, sk);
  SharedTransactions tr3({transactions[4], transactions[5]});
  // DAG block4
  propose_level = 3;
  vdf_sortition::VdfSortition vdf4(vdf_config, vrf_sk, VrfSortitionBase::makeVrfInput(propose_level, period_block_hash),
                                   1, 1);
  vdf_msg = DagManager::getVdfMessage(blk3.getHash(), {transactions[6], transactions[7]});
  vdf4.computeVdfSolution(vdf_config, vdf_msg, false);
  DagBlock blk4(blk3.getHash(), propose_level, {}, {transactions[6]->getHash(), transactions[7]->getHash()},
                2 * estimation, vdf4, sk);
  SharedTransactions tr4({transactions[6], transactions[7]});
  // DAG block5
  propose_level = 2;
  vdf_sortition::VdfSortition vdf5(vdf_config, vrf_sk, VrfSortitionBase::makeVrfInput(propose_level, period_block_hash),
                                   1, 1);
  vdf_msg = DagManager::getVdfMessage(blk2.getHash(), {transactions[8], transactions[9]});
  vdf5.computeVdfSolution(vdf_config, vdf_msg, false);
  DagBlock blk5(blk2.getHash(), propose_level, {}, {transactions[8]->getHash(), transactions[9]->getHash()},
                2 * estimation, vdf5, sk);
  SharedTransactions tr5({transactions[8], transactions[9]});
  // DAG block6
  propose_level = 2;
  vdf_sortition::VdfSortition vdf6(vdf_config, vrf_sk, VrfSortitionBase::makeVrfInput(propose_level, period_block_hash),
                                   1, 1);
  vdf_msg = DagManager::getVdfMessage(blk1.getHash(), {transactions[10], transactions[11]});
  vdf6.computeVdfSolution(vdf_config, vdf_msg, false);
  DagBlock blk6(blk1.getHash(), propose_level, {}, {transactions[10]->getHash(), transactions[11]->getHash()},
                2 * estimation, vdf6, sk);
  SharedTransactions tr6({transactions[10], transactions[11]});
  // DAG block7
  propose_level = 3;
  vdf_sortition::VdfSortition vdf7(vdf_config, vrf_sk, VrfSortitionBase::makeVrfInput(propose_level, period_block_hash),
                                   1, 1);
  vdf_msg = DagManager::getVdfMessage(blk6.getHash(), {transactions[12], transactions[13]});
  vdf7.computeVdfSolution(vdf_config, vdf_msg, false);
  DagBlock blk7(blk6.getHash(), propose_level, {}, {transactions[12]->getHash(), transactions[13]->getHash()},
                2 * estimation, vdf7, sk);
  SharedTransactions tr7({transactions[12], transactions[13]});
  // DAG block8
  propose_level = 4;
  vdf_sortition::VdfSortition vdf8(vdf_config, vrf_sk, VrfSortitionBase::makeVrfInput(propose_level, period_block_hash),
                                   1, 1);
  vdf_msg = DagManager::getVdfMessage(blk1.getHash(), {transactions[14], transactions[15]});
  vdf8.computeVdfSolution(vdf_config, vdf_msg, false);
  DagBlock blk8(blk1.getHash(), propose_level, {blk7.getHash()},
                {transactions[14]->getHash(), transactions[15]->getHash()}, 2 * estimation, vdf8, sk);
  SharedTransactions tr8({transactions[14], transactions[15]});
  // DAG block9
  propose_level = 2;
  vdf_sortition::VdfSortition vdf9(vdf_config, vrf_sk, VrfSortitionBase::makeVrfInput(propose_level, period_block_hash),
                                   1, 1);
  vdf_msg = DagManager::getVdfMessage(blk1.getHash(), {transactions[16], transactions[17]});
  vdf9.computeVdfSolution(vdf_config, vdf_msg, false);
  DagBlock blk9(blk1.getHash(), propose_level, {}, {transactions[16]->getHash(), transactions[17]->getHash()},
                2 * estimation, vdf9, sk);
  SharedTransactions tr9({transactions[16], transactions[17]});
  // DAG block10
  propose_level = 5;
  vdf_sortition::VdfSortition vdf10(vdf_config, vrf_sk,
                                    VrfSortitionBase::makeVrfInput(propose_level, period_block_hash), 1, 1);
  vdf_msg = DagManager::getVdfMessage(blk8.getHash(), {transactions[18], transactions[19]});
  vdf10.computeVdfSolution(vdf_config, vdf_msg, false);
  DagBlock blk10(blk8.getHash(), propose_level, {}, {transactions[18]->getHash(), transactions[19]->getHash()},
                 2 * estimation, vdf10, sk);
  SharedTransactions tr10({transactions[18], transactions[19]});
  // DAG block11
  propose_level = 3;
  vdf_sortition::VdfSortition vdf11(vdf_config, vrf_sk,
                                    VrfSortitionBase::makeVrfInput(propose_level, period_block_hash), 1, 1);
  vdf_msg = DagManager::getVdfMessage(blk3.getHash(), {transactions[20], transactions[21]});
  vdf11.computeVdfSolution(vdf_config, vdf_msg, false);
  DagBlock blk11(blk3.getHash(), propose_level, {}, {transactions[20]->getHash(), transactions[21]->getHash()},
                 2 * estimation, vdf11, sk);
  SharedTransactions tr11({transactions[20], transactions[21]});
  // DAG block12
  propose_level = 3;
  vdf_sortition::VdfSortition vdf12(vdf_config, vrf_sk,
                                    VrfSortitionBase::makeVrfInput(propose_level, period_block_hash), 1, 1);
  vdf_msg = DagManager::getVdfMessage(blk5.getHash(), {transactions[22], transactions[23]});
  vdf12.computeVdfSolution(vdf_config, vdf_msg, false);
  DagBlock blk12(blk5.getHash(), propose_level, {}, {transactions[22]->getHash(), transactions[23]->getHash()},
                 2 * estimation, vdf12, sk);
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
    for (auto t : trxs[i])
      node1->getTransactionManager()->insertValidatedTransaction(std::move(t), TransactionStatus::Verified);
    node1->getDagManager()->verifyBlock(std::move(blks[i]));
    node1->getDagManager()->addDagBlock(DagBlock(blks[i]));
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

  for (auto t : *g_signed_trx_samples)
    node1->getTransactionManager()->insertValidatedTransaction(std::shared_ptr(t), TransactionStatus::Verified);

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
  auto node_cfgs = make_node_cfgs(numberOfNodes, 1, 20);
  auto nodes = launch_nodes(slice(node_cfgs, 0, numberOfNodes - 1));

  std::random_device dev;
  std::mt19937 rng(dev());
  std::uniform_int_distribution<std::mt19937::result_type> distTransactions(1, 20);
  std::uniform_int_distribution<std::mt19937::result_type> distNodes(0, numberOfNodes - 2);  // range [0, 3]

  int num_of_trxs = 50;
  auto trxs = samples::createSignedTrxSamples(0, num_of_trxs, g_secret);
  for (auto i = 0; i < num_of_trxs; ++i) {
    nodes[distNodes(rng)]->getTransactionManager()->insertValidatedTransaction(std::move(trxs[i]),
                                                                               TransactionStatus::Verified);
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
    EXPECT_EQ(nodes[i]->getDagManager()->getNumVerticesInDag().first, nodes[i]->getDB()->getDagBlocksCount());
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
    EXPECT_EQ(nodes[i]->getDagManager()->getNumVerticesInDag().first, nodes[i]->getDB()->getDagBlocksCount());
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

  // This part of unit tests is commented out since it takes about one minute to actually test, run it if there are
  // any issues with this functionality

  /*thisThreadSleepForSeconds(60);
  for (int i = 0; i < 1000; i++) {
    EXPECT_FALSE(peer.reportSuspiciousPacket());
  }
  EXPECT_TRUE(peer.reportSuspiciousPacket());*/
}

TEST_F(NetworkTest, dag_syncing_limit) {
  network::tarcap::TaraxaPeer peer1, peer2;
  const uint64_t dag_sync_limit = 60;

  EXPECT_TRUE(peer1.dagSyncingAllowed());
  peer1.peer_dag_synced_ = true;
  peer1.peer_dag_synced_time_ =
      std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
  EXPECT_FALSE(peer1.dagSyncingAllowed());
  peer1.peer_dag_synced_time_ =
      std::chrono::duration_cast<std::chrono::seconds>(
          (std::chrono::system_clock::now() - std::chrono::seconds(dag_sync_limit - 1)).time_since_epoch())
          .count();
  EXPECT_FALSE(peer1.dagSyncingAllowed());
  peer1.peer_dag_synced_time_ =
      std::chrono::duration_cast<std::chrono::seconds>(
          (std::chrono::system_clock::now() - std::chrono::seconds(dag_sync_limit + 1)).time_since_epoch())
          .count();
  EXPECT_TRUE(peer1.dagSyncingAllowed());

  EXPECT_TRUE(peer2.requestDagSyncingAllowed());
  peer2.peer_requested_dag_syncing_ = true;
  peer2.peer_requested_dag_syncing_time_ =
      std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
  EXPECT_FALSE(peer2.requestDagSyncingAllowed());
  peer2.peer_requested_dag_syncing_time_ =
      std::chrono::duration_cast<std::chrono::seconds>(
          (std::chrono::system_clock::now() - std::chrono::seconds(dag_sync_limit - 1)).time_since_epoch())
          .count();
  EXPECT_FALSE(peer2.requestDagSyncingAllowed());
  peer2.peer_requested_dag_syncing_time_ =
      std::chrono::duration_cast<std::chrono::seconds>(
          (std::chrono::system_clock::now() - std::chrono::seconds(dag_sync_limit + 1)).time_since_epoch())
          .count();
  EXPECT_TRUE(peer2.requestDagSyncingAllowed());
}

TEST_F(NetworkTest, peer_cache_test) {
  const uint64_t max_cache_size = 200000;
  const uint64_t delete_step = 1000;
  const uint64_t max_block_to_keep_in_cache = 10;
  ExpirationBlockNumberCache<trx_hash_t> known_transactions_with_block_number(max_cache_size, delete_step,
                                                                              max_block_to_keep_in_cache);
  ExpirationCache<trx_hash_t> known_transactions(max_cache_size, delete_step);
  for (uint64_t block_number = 1; block_number < 1000; block_number++) {
    // Each block insert 2000 transactions
    const uint64_t transactions_in_block = 2000;
    for (uint64_t i = 0; i < transactions_in_block; i++) {
      known_transactions_with_block_number.insert(trx_hash_t(block_number * transactions_in_block + i), block_number);
      known_transactions.insert(trx_hash_t(block_number * transactions_in_block + i));
    }
    uint64_t number_of_insertion = block_number * transactions_in_block;
    uint64_t expected_known_transactions = std::min(number_of_insertion, max_cache_size);
    EXPECT_EQ(known_transactions.size(), expected_known_transactions);
    uint64_t expected_known_transactions_with_block_number =
        std::min(number_of_insertion, (max_block_to_keep_in_cache + 1) * transactions_in_block);
    EXPECT_EQ(known_transactions_with_block_number.size(), expected_known_transactions_with_block_number);

    std::cout << block_number << " " << known_transactions_with_block_number.size() << " " << known_transactions.size()
              << std::endl;
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
