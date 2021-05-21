#include "consensus/pbft_manager.hpp"

#include <gtest/gtest.h>

#include "common/static_init.hpp"
#include "logger/log.hpp"
#include "network/network.hpp"
#include "util/lazy.hpp"
#include "util_test/samples.hpp"
#include "util_test/util.hpp"

namespace taraxa::core_tests {

const unsigned NUM_TRX = 200;
auto g_secret = Lazy([] {
  return dev::Secret("3800b2875669d9b2053c1aff9224ecfdc411423aac5b5a73d7a45ced1c3b9dcd",
                     dev::Secret::ConstructFromStringType::FromHex);
});
auto g_key_pair = Lazy([] { return dev::KeyPair(g_secret); });
auto g_trx_signed_samples = Lazy([] { return samples::createSignedTrxSamples(0, NUM_TRX, g_secret); });
auto g_mock_dag0 = Lazy([] { return samples::createMockDag0(); });

struct PbftManagerTest : BaseTest {};

pair<size_t, size_t> calculate_2tPuls1_threshold(size_t committee_size, size_t valid_voting_players) {
  size_t two_t_plus_one;
  size_t threshold;
  if (committee_size <= valid_voting_players) {
    two_t_plus_one = committee_size * 2 / 3 + 1;
    // round up
    threshold = (valid_voting_players * committee_size - 1) / valid_voting_players + 1;
  } else {
    two_t_plus_one = valid_voting_players * 2 / 3 + 1;
    threshold = valid_voting_players;
  }
  return make_pair(two_t_plus_one, threshold);
}

void check_2tPlus1_validVotingPlayers_activePlayers_threshold(size_t committee_size) {
  auto node_cfgs = make_node_cfgs<20>(5);
  auto node_1_expected_bal = own_effective_genesis_bal(node_cfgs[0]);
  for (auto &cfg : node_cfgs) {
    cfg.chain.pbft.committee_size = committee_size;
  }
  auto nodes = launch_nodes(node_cfgs);

  // Even distribute coins from master boot node to other nodes. Since master
  // boot node owns whole coins, the active players should be only master boot
  // node at the moment.
  auto gas_price = val_t(2);
  auto data = bytes();
  auto nonce = 0;  // fixme: the following nonce approach is not correct anyway
  uint64_t trxs_count = 0;

  {
    auto min_stake_to_vote = node_cfgs[0].chain.final_chain.state.dpos->eligibility_balance_threshold;
    state_api::DPOSTransfers delegations;
    for (size_t i(1); i < nodes.size(); ++i) {
      node_1_expected_bal -= delegations[nodes[i]->getAddress()].value = min_stake_to_vote;
    }
    auto trx = make_dpos_trx(node_cfgs[0], delegations, nonce++);
    nodes[0]->getTransactionManager()->insertTransaction(trx, false, true);
    trxs_count++;
    EXPECT_HAPPENS({60s, 1s}, [&](auto &ctx) {
      for (auto &node : nodes) {
        if (ctx.fail_if(!node->getFinalChain()->isKnownTransaction(trx.getHash()))) {
          return;
        }
      }
    });
  }

  auto init_bal = node_1_expected_bal / nodes.size();
  for (size_t i(1); i < nodes.size(); ++i) {
    Transaction master_boot_node_send_coins(nonce++, init_bal, gas_price, TEST_TX_GAS_LIMIT, data,
                                            nodes[0]->getSecretKey(), nodes[i]->getAddress());
    node_1_expected_bal -= init_bal;
    // broadcast trx and insert
    nodes[0]->getTransactionManager()->insertTransaction(master_boot_node_send_coins, false, true);
    trxs_count++;
  }

  std::cout << "Checking all nodes executed transactions from master boot node" << std::endl;
  EXPECT_HAPPENS({80s, 8s}, [&](auto &ctx) {
    for (size_t i(0); i < nodes.size(); ++i) {
      if (nodes[i]->getDB()->getNumTransactionExecuted() != trxs_count) {
        std::cout << "node" << i << " executed " << nodes[i]->getDB()->getNumTransactionExecuted()
                  << " transactions, expected " << trxs_count << std::endl;
        Transaction dummy_trx(nonce++, 0, 2, TEST_TX_GAS_LIMIT, bytes(), nodes[0]->getSecretKey(),
                              nodes[0]->getAddress());
        // broadcast dummy transaction
        nodes[0]->getTransactionManager()->insertTransaction(dummy_trx, false, true);
        trxs_count++;
        ctx.fail();
        return;
      }
    }
  });
  for (size_t i(0); i < nodes.size(); ++i) {
    EXPECT_EQ(nodes[i]->getDB()->getNumTransactionExecuted(), trxs_count);
  }

  for (size_t i(0); i < nodes.size(); ++i) {
    std::cout << "Checking account balances on node " << i << " ..." << std::endl;
    EXPECT_EQ(nodes[i]->getFinalChain()->getBalance(nodes[0]->getAddress()).first, node_1_expected_bal);
    for (size_t j(1); j < nodes.size(); ++j) {
      // For node1 to node4 balances info on each node
      EXPECT_EQ(nodes[i]->getFinalChain()->getBalance(nodes[j]->getAddress()).first, init_bal);
    }
  }
  uint64_t valid_voting_players = 0;
  size_t committee, two_t_plus_one, threshold, expected_2tPlus1, expected_threshold;
  for (size_t i(0); i < nodes.size(); ++i) {
    auto pbft_mgr = nodes[i]->getPbftManager();
    committee = pbft_mgr->getPbftCommitteeSize();
    valid_voting_players = pbft_mgr->getDposTotalVotesCount();
    two_t_plus_one = pbft_mgr->getTwoTPlusOne();
    threshold = pbft_mgr->getSortitionThreshold();
    std::cout << "Node" << i << " committee " << committee << ", valid voting players " << valid_voting_players
              << ", 2t+1 " << two_t_plus_one << ", sortition threshold " << threshold << std::endl;
    EXPECT_EQ(valid_voting_players, nodes.size());
    tie(expected_2tPlus1, expected_threshold) = calculate_2tPuls1_threshold(committee, valid_voting_players);
    EXPECT_EQ(two_t_plus_one, expected_2tPlus1);
    EXPECT_EQ(threshold, expected_threshold);
  }

  auto send_coins = 1;
  for (size_t i(0); i < nodes.size(); ++i) {
    // Sending coins in Robin Cycle in order to make all nodes to be active
    // players, but not guarantee
    auto receiver_index = (i + 1) % nodes.size();
    Transaction send_coins_in_robin_cycle(nonce++, send_coins, gas_price, TEST_TX_GAS_LIMIT, data,
                                          nodes[i]->getSecretKey(), nodes[receiver_index]->getAddress());
    // broadcast trx and insert
    nodes[i]->getTransactionManager()->insertTransaction(send_coins_in_robin_cycle, false, true);
    trxs_count++;
  }

  std::cout << "Checking all nodes execute transactions from robin cycle" << std::endl;
  EXPECT_HAPPENS({80s, 8s}, [&](auto &ctx) {
    for (size_t i(0); i < nodes.size(); ++i) {
      if (nodes[i]->getDB()->getNumTransactionExecuted() != trxs_count) {
        std::cout << "node" << i << " executed " << nodes[i]->getDB()->getNumTransactionExecuted()
                  << " transactions. Expected " << trxs_count << std::endl;
        Transaction dummy_trx(nonce++, 0, 2, TEST_TX_GAS_LIMIT, bytes(), nodes[0]->getSecretKey(),
                              nodes[0]->getAddress());
        // broadcast dummy transaction
        nodes[0]->getTransactionManager()->insertTransaction(dummy_trx, false, true);
        trxs_count++;
        ctx.fail();
        return;
      }
    }
  });
  for (size_t i = 0; i < nodes.size(); i++) {
    EXPECT_EQ(nodes[i]->getDB()->getNumTransactionExecuted(), trxs_count);
  }
  // Account balances should not change in robin cycle
  for (size_t i(0); i < nodes.size(); ++i) {
    std::cout << "Checking account balances on node " << i << " ..." << std::endl;
    EXPECT_EQ(nodes[i]->getFinalChain()->getBalance(nodes[0]->getAddress()).first, node_1_expected_bal);
    for (size_t j(1); j < nodes.size(); ++j) {
      // For node1 to node4 account balances info on each node
      EXPECT_EQ(nodes[i]->getFinalChain()->getBalance(nodes[j]->getAddress()).first, init_bal);
    }
  }

  for (size_t i(0); i < nodes.size(); ++i) {
    auto pbft_mgr = nodes[i]->getPbftManager();
    committee = pbft_mgr->getPbftCommitteeSize();
    valid_voting_players = pbft_mgr->getDposTotalVotesCount();
    two_t_plus_one = pbft_mgr->getTwoTPlusOne();
    threshold = pbft_mgr->getSortitionThreshold();
    std::cout << "Node" << i << " committee " << committee << ", valid voting players " << valid_voting_players
              << ", 2t+1 " << two_t_plus_one << ", sortition threshold " << threshold << std::endl;
    EXPECT_EQ(valid_voting_players, nodes.size());
    tie(expected_2tPlus1, expected_threshold) = calculate_2tPuls1_threshold(committee, valid_voting_players);
    EXPECT_EQ(two_t_plus_one, expected_2tPlus1);
    EXPECT_EQ(threshold, expected_threshold);
  }
}

TEST_F(PbftManagerTest, full_node_lambda_input_test) {
  auto node_cfgs = make_node_cfgs(1);
  FullNode::Handle node(node_cfgs[0]);

  node->start();
  auto pbft_mgr = node->getPbftManager();
  EXPECT_EQ(pbft_mgr->getPbftInitialLambda(), 2000);
}

TEST_F(PbftManagerTest, check_get_eligible_vote_count) {
  auto node_cfgs = make_node_cfgs<5>(5);
  auto node_1_expected_bal = own_effective_genesis_bal(node_cfgs[0]);
  for (auto &cfg : node_cfgs) {
    cfg.chain.pbft.committee_size = 100;
  }
  auto nodes = launch_nodes(node_cfgs);

  // Even distribute coins from master boot node to other nodes. Since master
  // boot node owns whole coins, the active players should be only master boot
  // node at the moment.
  auto gas_price = val_t(2);
  auto data = bytes();
  auto nonce = 0;  // fixme: the following nonce approach is not correct anyway
  uint64_t trxs_count = 0;

  auto expected_eligible_total_vote = 1;
  auto curent_votes_for_node = 1;

  {
    auto min_stake_to_vote = node_cfgs[0].chain.final_chain.state.dpos->eligibility_balance_threshold;
    auto stake_to_vote = min_stake_to_vote;
    state_api::DPOSTransfers delegations;
    for (size_t i(1); i < nodes.size(); ++i) {
      stake_to_vote += min_stake_to_vote;
      curent_votes_for_node++;
      expected_eligible_total_vote += curent_votes_for_node;
      std::cout << "Delegating stake of " << stake_to_vote << " to node " << i << std::endl;
      node_1_expected_bal -= delegations[nodes[i]->getAddress()].value = stake_to_vote;
    }
    auto trx = make_dpos_trx(node_cfgs[0], delegations, nonce++);
    nodes[0]->getTransactionManager()->insertTransaction(trx, false, true);

    trxs_count++;
    EXPECT_HAPPENS({120s, 1s}, [&](auto &ctx) {
      for (auto &node : nodes) {
        if (ctx.fail_if(!node->getFinalChain()->isKnownTransaction(trx.getHash()))) {
          return;
        }
      }
    });
  }

  auto init_bal = node_1_expected_bal / nodes.size() / 2;
  for (size_t i(1); i < nodes.size(); ++i) {
    Transaction master_boot_node_send_coins(nonce++, init_bal, gas_price, TEST_TX_GAS_LIMIT, data,
                                            nodes[0]->getSecretKey(), nodes[i]->getAddress());
    node_1_expected_bal -= init_bal;
    // broadcast trx and insert
    nodes[0]->getTransactionManager()->insertTransaction(master_boot_node_send_coins, false, true);
    trxs_count++;
  }

  std::cout << "Checking all nodes executed transactions from master boot node" << std::endl;
  EXPECT_HAPPENS({80s, 8s}, [&](auto &ctx) {
    for (size_t i(0); i < nodes.size(); ++i) {
      if (nodes[i]->getDB()->getNumTransactionExecuted() != trxs_count) {
        std::cout << "node" << i << " executed " << nodes[i]->getDB()->getNumTransactionExecuted()
                  << " transactions, expected " << trxs_count << std::endl;
        Transaction dummy_trx(nonce++, 0, 2, TEST_TX_GAS_LIMIT, bytes(), nodes[0]->getSecretKey(),
                              nodes[0]->getAddress());
        // broadcast dummy transaction
        nodes[0]->getTransactionManager()->insertTransaction(dummy_trx, false, true);
        trxs_count++;
        ctx.fail();
        return;
      }
    }
  });
  for (size_t i(0); i < nodes.size(); ++i) {
    EXPECT_EQ(nodes[i]->getDB()->getNumTransactionExecuted(), trxs_count);
  }

  for (size_t i(0); i < nodes.size(); ++i) {
    std::cout << "Checking account balances on node " << i << " ..." << std::endl;
    EXPECT_EQ(nodes[i]->getFinalChain()->getBalance(nodes[0]->getAddress()).first, node_1_expected_bal);
    for (size_t j(1); j < nodes.size(); ++j) {
      // For node1 to node4 balances info on each node
      EXPECT_EQ(nodes[i]->getFinalChain()->getBalance(nodes[j]->getAddress()).first, init_bal);
    }
  }

  auto send_coins = 1;
  for (size_t i(0); i < nodes.size(); ++i) {
    // Sending coins in Robin Cycle in order to make all nodes to be active
    // players, but not guarantee
    auto receiver_index = (i + 1) % nodes.size();
    Transaction send_coins_in_robin_cycle(nonce++, send_coins, gas_price, TEST_TX_GAS_LIMIT, data,
                                          nodes[i]->getSecretKey(), nodes[receiver_index]->getAddress());
    // broadcast trx and insert
    nodes[i]->getTransactionManager()->insertTransaction(send_coins_in_robin_cycle, false, true);
    trxs_count++;
  }

  std::cout << "Checking all nodes execute transactions from robin cycle" << std::endl;
  EXPECT_HAPPENS({80s, 8s}, [&](auto &ctx) {
    for (size_t i(0); i < nodes.size(); ++i) {
      if (nodes[i]->getDB()->getNumTransactionExecuted() != trxs_count) {
        std::cout << "node" << i << " executed " << nodes[i]->getDB()->getNumTransactionExecuted()
                  << " transactions. Expected " << trxs_count << std::endl;
        Transaction dummy_trx(nonce++, 0, 2, TEST_TX_GAS_LIMIT, bytes(), nodes[0]->getSecretKey(),
                              nodes[0]->getAddress());
        // broadcast dummy transaction
        nodes[0]->getTransactionManager()->insertTransaction(dummy_trx, false);
        trxs_count++;
        ctx.fail();
        return;
      }
    }
  });
  for (size_t i = 0; i < nodes.size(); i++) {
    EXPECT_EQ(nodes[i]->getDB()->getNumTransactionExecuted(), trxs_count);
  }
  // Account balances should not change in robin cycle
  for (size_t i(0); i < nodes.size(); ++i) {
    std::cout << "Checking account balances on node " << i << " ..." << std::endl;
    EXPECT_EQ(nodes[i]->getFinalChain()->getBalance(nodes[0]->getAddress()).first, node_1_expected_bal);
    for (size_t j(1); j < nodes.size(); ++j) {
      // For node1 to node4 account balances info on each node
      EXPECT_EQ(nodes[i]->getFinalChain()->getBalance(nodes[j]->getAddress()).first, init_bal);
    }
  }

  uint64_t eligible_total_vote_count = 0;
  size_t committee, two_t_plus_one, threshold, expected_2tPlus1, expected_threshold;
  for (size_t i(0); i < nodes.size(); ++i) {
    auto pbft_mgr = nodes[i]->getPbftManager();
    committee = pbft_mgr->getPbftCommitteeSize();
    eligible_total_vote_count = pbft_mgr->getDposTotalVotesCount();
    two_t_plus_one = pbft_mgr->getTwoTPlusOne();
    threshold = pbft_mgr->getSortitionThreshold();
    std::cout << "Node" << i << " committee " << committee << ", eligible total vote count "
              << eligible_total_vote_count << ", 2t+1 " << two_t_plus_one << ", sortition threshold " << threshold
              << std::endl;
    EXPECT_EQ(eligible_total_vote_count, expected_eligible_total_vote);
    tie(expected_2tPlus1, expected_threshold) = calculate_2tPuls1_threshold(committee, eligible_total_vote_count);
    EXPECT_EQ(two_t_plus_one, expected_2tPlus1);
    EXPECT_EQ(threshold, expected_threshold);
  }
}

TEST_F(PbftManagerTest, pbft_manager_run_single_node) {
  auto node_cfgs = make_node_cfgs<20>(1);
  FullNode::Handle node(node_cfgs[0], true);

  // create a transaction
  auto coins_value = val_t(100);
  auto gas_price = val_t(2);
  auto receiver = addr_t("973ecb1c08c8eb5a7eaa0d3fd3aab7924f2838b0");
  auto data = bytes();
  Transaction trx_master_boot_node_to_receiver(0, coins_value, gas_price, TEST_TX_GAS_LIMIT, data, node->getSecretKey(),
                                               receiver);
  node->getTransactionManager()->insertTransaction(trx_master_boot_node_to_receiver, false, true);

  for (auto _(0); _ < 120; ++_) {
    // test timeout is 60 seconds
    if (node->getNumProposedBlocks() == 1) {
      break;
    }
    taraxa::thisThreadSleepForMilliSeconds(500);
  }
  EXPECT_EQ(node->getNumProposedBlocks(), 1);

  std::shared_ptr<PbftChain> pbft_chain = node->getPbftChain();
  // Vote DAG block
  uint64_t pbft_chain_size = 1;
  for (auto _(0); _ < 120; ++_) {
    // test timeout is 60 seconds
    if (pbft_chain->getPbftChainSize() == pbft_chain_size) {
      break;
    }
    taraxa::thisThreadSleepForMilliSeconds(500);
  }
  EXPECT_EQ(pbft_chain->getPbftChainSize(), pbft_chain_size);

  for (auto _(0); _ < 120; ++_) {
    // test timeout is 60 seconds
    if (node->getDB()->getNumTransactionExecuted() != 0) {
      break;
    }
    taraxa::thisThreadSleepForMilliSeconds(500);
  }

  std::cout << "Checking nodes sees 1 transaction..." << std::endl;
  ASSERT_EQ(node->getDB()->getNumTransactionExecuted(), 1);
  EXPECT_EQ(node->getFinalChain()->getBalance(addr_t("de2b1203d72d3549ee2f733b00b2789414c7cea5")).first,
            own_effective_genesis_bal(node_cfgs[0]) - 100);
  EXPECT_EQ(node->getFinalChain()->getBalance(receiver).first, 100);
}

TEST_F(PbftManagerTest, pbft_manager_run_multi_nodes) {
  auto node_cfgs = make_node_cfgs<20>(3);
  auto node1_genesis_bal = own_effective_genesis_bal(node_cfgs[0]);
  auto nodes = launch_nodes(node_cfgs);

  auto node1_addr = nodes[0]->getAddress();
  auto node2_addr = nodes[1]->getAddress();
  auto node3_addr = nodes[2]->getAddress();

  // create a transaction transfer coins from node1 to node2
  auto coins_value2 = val_t(100);
  auto gas_price = val_t(2);
  auto data = bytes();
  Transaction trx_master_boot_node_to_node2(0, coins_value2, gas_price, TEST_TX_GAS_LIMIT, data,
                                            nodes[0]->getSecretKey(), node2_addr);
  // broadcast trx and insert
  nodes[0]->getTransactionManager()->insertTransaction(trx_master_boot_node_to_node2, false, true);

  std::cout << "Checking all nodes see transaction from node 1 to node 2..." << std::endl;

  bool checkpoint_passed = false;
  for (auto _(0); _ < 120; ++_) {
    checkpoint_passed = true;
    // test timeout is 60 seconds
    for (size_t i(0); i < nodes.size(); ++i) {
      if (nodes[i]->getDB()->getNumTransactionExecuted() == 0) {
        checkpoint_passed = false;
      }
    }
    if (checkpoint_passed) break;
    taraxa::thisThreadSleepForMilliSeconds(500);
  }

  if (checkpoint_passed == false) {
    for (size_t i(0); i < nodes.size(); ++i) {
      ASSERT_EQ(nodes[i]->getDB()->getNumTransactionExecuted(), 1);
    }
  }

  uint64_t pbft_chain_size = 1;
  for (size_t i(0); i < nodes.size(); ++i) {
    EXPECT_EQ(nodes[i]->getFinalChain()->last_block_number(), pbft_chain_size);
  }

  for (size_t i(0); i < nodes.size(); ++i) {
    std::cout << "Checking account balances on node " << i << " ..." << std::endl;
    EXPECT_EQ(nodes[i]->getFinalChain()->getBalance(node1_addr).first, node1_genesis_bal - 100);
    EXPECT_EQ(nodes[i]->getFinalChain()->getBalance(node2_addr).first, 100);
    EXPECT_EQ(nodes[i]->getFinalChain()->getBalance(node3_addr).first, 0);
  }

  // create a transaction transfer coins from node1 to node3
  auto coins_value3 = val_t(1000);
  Transaction trx_master_boot_node_to_node3(1, coins_value3, gas_price, TEST_TX_GAS_LIMIT, data,
                                            nodes[0]->getSecretKey(), node3_addr);
  // broadcast trx and insert
  nodes[0]->getTransactionManager()->insertTransaction(trx_master_boot_node_to_node3, false, true);

  std::cout << "Checking all nodes see transaction from node 1 to node 3..." << std::endl;

  checkpoint_passed = false;
  for (auto _(0); _ < 120; ++_) {
    checkpoint_passed = true;
    // test timeout is 60 seconds
    for (size_t i(0); i < nodes.size(); ++i) {
      if (nodes[i]->getDB()->getNumTransactionExecuted() != 2) {
        checkpoint_passed = false;
      }
    }
    if (checkpoint_passed) break;
    taraxa::thisThreadSleepForMilliSeconds(500);
  }

  if (checkpoint_passed == false) {
    for (size_t i(0); i < nodes.size(); ++i) {
      ASSERT_EQ(nodes[i]->getDB()->getNumTransactionExecuted(), 1);
    }
  }

  pbft_chain_size = 2;
  // Vote DAG block
  for (auto _(0); _ < 120; ++_) {
    // test timeout is 60 seconds
    if (nodes[0]->getFinalChain()->last_block_number() == pbft_chain_size &&
        nodes[1]->getFinalChain()->last_block_number() == pbft_chain_size &&
        nodes[2]->getFinalChain()->last_block_number() == pbft_chain_size) {
      break;
    }
    taraxa::thisThreadSleepForMilliSeconds(500);
  }
  for (size_t i(0); i < nodes.size(); ++i) {
    EXPECT_EQ(nodes[i]->getFinalChain()->last_block_number(), pbft_chain_size);
  }

  for (size_t i(0); i < nodes.size(); ++i) {
    std::cout << "Checking account balances on node " << i << " ..." << std::endl;
    EXPECT_EQ(nodes[i]->getFinalChain()->getBalance(node1_addr).first, node1_genesis_bal - 1100);
    EXPECT_EQ(nodes[i]->getFinalChain()->getBalance(node2_addr).first, 100);
    EXPECT_EQ(nodes[i]->getFinalChain()->getBalance(node3_addr).first, 1000);
  }
  // PBFT second block
  blk_hash_t pbft_second_block_hash = nodes[0]->getPbftChain()->getLastPbftBlockHash();
  PbftBlock pbft_second_block = nodes[0]->getPbftChain()->getPbftBlockInChain(pbft_second_block_hash);
  for (auto &node : nodes) {
    auto db = node->getDB();
    auto dag_blocks_hash_in_schedule = db->getFinalizedDagBlockHashesByAnchor(pbft_second_block.getPivotDagBlockHash());
    // due to change of trx packing change, a trx can be packed in multiple
    // blocks
    std::unordered_set<blk_hash_t> unique_dag_block_hash_set;
    EXPECT_GE(dag_blocks_hash_in_schedule.size(), 1);
    for (auto &dag_block_hash : dag_blocks_hash_in_schedule) {
      ASSERT_FALSE(unique_dag_block_hash_set.count(dag_block_hash));
      unique_dag_block_hash_set.insert(dag_block_hash);
    }
    // PBFT first block hash
    blk_hash_t pbft_first_block_hash = pbft_second_block.getPrevBlockHash();
    // PBFT first block
    PbftBlock pbft_first_block = nodes[0]->getPbftChain()->getPbftBlockInChain(pbft_first_block_hash);
    dag_blocks_hash_in_schedule = db->getFinalizedDagBlockHashesByAnchor(pbft_first_block.getPivotDagBlockHash());
    // due to change of trx packing change, a trx can be packed in multiple
    // blocks
    EXPECT_GE(dag_blocks_hash_in_schedule.size(), 1);
    for (auto &dag_block_hash : dag_blocks_hash_in_schedule) {
      ASSERT_FALSE(unique_dag_block_hash_set.count(dag_block_hash));
      unique_dag_block_hash_set.insert(dag_block_hash);
    }
  }
}

TEST_F(PbftManagerTest, check_committeeSize_less_or_equal_to_activePlayers) {
  // Set committee size to 5, make sure to be committee <= active_players
  check_2tPlus1_validVotingPlayers_activePlayers_threshold(5);
}

TEST_F(PbftManagerTest, check_committeeSize_greater_than_activePlayers) {
  // Set committee size to 6. Since only running 5 nodes, that will make sure
  // committee > active_players always
  check_2tPlus1_validVotingPlayers_activePlayers_threshold(6);
}

}  // namespace taraxa::core_tests

using namespace taraxa;
int main(int argc, char **argv) {
  taraxa::static_init();
  auto logging = logger::createDefaultLoggingConfig();
  logging.verbosity = logger::Verbosity::Error;

  addr_t node_addr;
  logger::InitLogging(logging, node_addr);

  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
