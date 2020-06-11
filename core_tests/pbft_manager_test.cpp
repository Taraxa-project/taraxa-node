#include "pbft_manager.hpp"

#include <gtest/gtest.h>

#include "core_tests/util.hpp"
#include "create_samples.hpp"
#include "network.hpp"
#include "static_init.hpp"
#include "util/lazy.hpp"
#include "util/wait.hpp"

namespace taraxa {
using namespace core_tests::util;
using core_tests::util::constants::TEST_TX_GAS_LIMIT;
using ::taraxa::util::lazy::Lazy;

const unsigned NUM_TRX = 200;
auto g_secret = Lazy([] {
  return dev::Secret(
      "3800b2875669d9b2053c1aff9224ecfdc411423aac5b5a73d7a45ced1c3b9dcd",
      dev::Secret::ConstructFromStringType::FromHex);
});
auto g_key_pair = Lazy([] { return dev::KeyPair(g_secret); });
auto g_trx_signed_samples =
    Lazy([] { return samples::createSignedTrxSamples(0, NUM_TRX, g_secret); });
auto g_mock_dag0 = Lazy([] { return samples::createMockDag0(); });

struct PbftManagerTest : core_tests::util::DBUsingTest<> {};

pair<size_t, size_t> calculate_2tPuls1_threshold(size_t committee_size,
                                                 size_t active_players,
                                                 size_t valid_voting_players) {
  size_t two_t_plus_one;
  size_t threshold;
  if (committee_size <= active_players) {
    two_t_plus_one = committee_size * 2 / 3 + 1;
    // round up
    threshold =
        (valid_voting_players * committee_size - 1) / active_players + 1;
  } else {
    two_t_plus_one = active_players * 2 / 3 + 1;
    threshold = valid_voting_players;
  }
  return make_pair(two_t_plus_one, threshold);
}

void check_2tPlus1_validVotingPlayers_activePlayers_threshold(
    size_t committee_size) {
  auto tops = createNodesAndVerifyConnection(5, 4, false, 20);
  auto &nodes = tops.second;

  std::shared_ptr<PbftManager> pbft_mgr;
  for (auto i(0); i < nodes.size(); ++i) {
    pbft_mgr = nodes[i]->getPbftManager();
    pbft_mgr->COMMITTEE_SIZE = committee_size;
    // Set PBFT skip periods to a large number, in order to count all nodes as
    // active players (not guarantee)
    pbft_mgr->SKIP_PERIODS = 100;
  }

  // Even distribute coins from master boot node to other nodes. Since master
  // boot node owns whole coins, the active players should be only master boot
  // node at the moment.
  auto init_bal = TARAXA_COINS_DECIMAL / nodes.size();
  auto gas_price = val_t(2);
  auto data = bytes();
  auto nonce = 0;
  auto trxs_count = 0;
  for (auto i(1); i < nodes.size(); ++i) {
    Transaction master_boot_node_send_coins(
        nonce++, init_bal, gas_price, TEST_TX_GAS_LIMIT, data,
        nodes[0]->getSecretKey(), nodes[i]->getAddress());
    // broadcast trx and insert
    nodes[0]->insertTransaction(master_boot_node_send_coins, false);
    trxs_count++;
  }

  std::cout << "Checking all nodes executed transactions from master boot node"
            << std::endl;
  auto success = wait::Wait(
      [&nodes, &trxs_count, &nonce] {
        for (auto i(0); i < nodes.size(); ++i) {
          if (nodes[i]->getNumTransactionExecuted() != trxs_count) {
            std::cout << "node" << i << " executed "
                      << nodes[i]->getNumTransactionExecuted()
                      << " transactions, expected " << trxs_count << std::endl;
            Transaction dummy_trx(nonce++, 0, 2, TEST_TX_GAS_LIMIT, bytes(),
                                  nodes[0]->getSecretKey(),
                                  nodes[0]->getAddress());
            // broadcast dummy transaction
            nodes[0]->insertTransaction(dummy_trx, false);
            trxs_count++;
            return false;
          }
        }
        return true;
      },
      {
          10,                       // send times
          std::chrono::seconds(2),  // each sending
      });
  for (auto i(0); i < nodes.size(); ++i) {
    EXPECT_EQ(nodes[i]->getNumTransactionExecuted(), trxs_count);
  }

  for (auto i(0); i < nodes.size(); ++i) {
    std::cout << "Checking account balances on node " << i << " ..."
              << std::endl;
    EXPECT_EQ(nodes[i]->getBalance(nodes[0]->getAddress()).first,
              9007199254740991 - 4 * init_bal);
    for (auto j(1); j < nodes.size(); ++j) {
      // For node1 to node4 balances info on each node
      EXPECT_EQ(nodes[i]->getBalance(nodes[j]->getAddress()).first, init_bal);
    }
  }

  size_t committee, active_players, valid_voting_players, two_t_plus_one,
      threshold, expected_2tPlus1, expected_threshold;
  for (auto i(0); i < nodes.size(); ++i) {
    pbft_mgr = nodes[i]->getPbftManager();
    committee = pbft_mgr->COMMITTEE_SIZE;
    active_players = pbft_mgr->active_nodes;
    valid_voting_players = pbft_mgr->getValidSortitionAccountsSize();
    two_t_plus_one = pbft_mgr->getTwoTPlusOne();
    threshold = pbft_mgr->getSortitionThreshold();
    std::cout << "Node" << i << " committee " << committee
              << ", active players " << active_players
              << ", valid voting players " << valid_voting_players << ", 2t+1 "
              << two_t_plus_one << ", sortition threshold " << threshold
              << std::endl;
    EXPECT_EQ(valid_voting_players, nodes.size());
    tie(expected_2tPlus1, expected_threshold) = calculate_2tPuls1_threshold(
        committee, active_players, valid_voting_players);
    EXPECT_EQ(two_t_plus_one, expected_2tPlus1);
    EXPECT_EQ(threshold, expected_threshold);
  }

  auto send_coins = 1;
  for (auto i(0); i < nodes.size(); ++i) {
    // Sending coins in Robin Cycle in order to make all nodes to be active
    // players, but not guarantee
    auto receiver_index = (i + 1) % nodes.size();
    Transaction send_coins_in_robin_cycle(
        nonce++, send_coins, gas_price, TEST_TX_GAS_LIMIT, data,
        nodes[i]->getSecretKey(), nodes[receiver_index]->getAddress());
    // broadcast trx and insert
    nodes[i]->insertTransaction(send_coins_in_robin_cycle, false);
    trxs_count++;
  }

  std::cout << "Checking all nodes execute transactions from robin cycle"
            << std::endl;
  success = wait::Wait(
      [&nodes, &trxs_count, &nonce] {
        for (auto i(0); i < nodes.size(); ++i) {
          if (nodes[i]->getNumTransactionExecuted() != trxs_count) {
            std::cout << "node" << i << " executed "
                      << nodes[i]->getNumTransactionExecuted()
                      << " transactions. Expected " << trxs_count << std::endl;
            Transaction dummy_trx(nonce++, 0, 2, TEST_TX_GAS_LIMIT, bytes(),
                                  nodes[0]->getSecretKey(),
                                  nodes[0]->getAddress());
            // broadcast dummy transaction
            nodes[0]->insertTransaction(dummy_trx, false);
            trxs_count++;
            return false;
          }
        }
        return true;
      },
      {
          10,                       // send times
          std::chrono::seconds(2),  // each sending
      });
  for (auto i = 0; i < nodes.size(); i++) {
    EXPECT_EQ(nodes[i]->getNumTransactionExecuted(), trxs_count);
  }
  // Account balances should not change in robin cycle
  for (auto i(0); i < nodes.size(); ++i) {
    std::cout << "Checking account balances on node " << i << " ..."
              << std::endl;
    EXPECT_EQ(nodes[i]->getBalance(nodes[0]->getAddress()).first,
              9007199254740991 - 4 * init_bal);
    for (auto j(1); j < nodes.size(); ++j) {
      // For node1 to node4 account balances info on each node
      EXPECT_EQ(nodes[i]->getBalance(nodes[j]->getAddress()).first, init_bal);
    }
  }

  for (auto i(0); i < nodes.size(); ++i) {
    pbft_mgr = nodes[i]->getPbftManager();
    committee = pbft_mgr->COMMITTEE_SIZE;
    active_players = pbft_mgr->active_nodes;
    valid_voting_players = pbft_mgr->getValidSortitionAccountsSize();
    two_t_plus_one = pbft_mgr->getTwoTPlusOne();
    threshold = pbft_mgr->getSortitionThreshold();
    std::cout << "Node" << i << " committee " << committee
              << ", active players " << active_players
              << ", valid voting players " << valid_voting_players << ", 2t+1 "
              << two_t_plus_one << ", sortition threshold " << threshold
              << std::endl;
    EXPECT_EQ(valid_voting_players, nodes.size());
    tie(expected_2tPlus1, expected_threshold) = calculate_2tPuls1_threshold(
        committee, active_players, valid_voting_players);
    EXPECT_EQ(two_t_plus_one, expected_2tPlus1);
    EXPECT_EQ(threshold, expected_threshold);
  }
}

TEST_F(PbftManagerTest, pbft_manager_run_single_node) {
  auto tops = createNodesAndVerifyConnection(1, 0, false, 20);
  auto node = tops.second[0];

  // create a transaction
  auto coins_value = val_t(100);
  auto gas_price = val_t(2);
  auto receiver = addr_t("973ecb1c08c8eb5a7eaa0d3fd3aab7924f2838b0");
  auto data = bytes();
  Transaction trx_master_boot_node_to_receiver(0, coins_value, gas_price,
                                               TEST_TX_GAS_LIMIT, data,
                                               node->getSecretKey(), receiver);
  node->insertTransaction(trx_master_boot_node_to_receiver, false);

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
  int pbft_chain_size = 1;
  for (auto _(0); _ < 120; ++_) {
    // test timeout is 60 seconds
    if (pbft_chain->getPbftChainSize() == pbft_chain_size) {
      break;
    }
    taraxa::thisThreadSleepForMilliSeconds(500);
  }
  EXPECT_EQ(pbft_chain->getPbftChainSize(), pbft_chain_size);

  std::cout << "Checking nodes sees 1 transaction..." << std::endl;
  ASSERT_EQ(node->getNumTransactionExecuted(), 1);
  EXPECT_EQ(node->getBalance(addr_t("de2b1203d72d3549ee2f733b00b2789414c7cea5"))
                .first,
            9007199254740991 - 100);
  EXPECT_EQ(node->getBalance(receiver).first, 100);
}

TEST_F(PbftManagerTest, pbft_manager_run_multi_nodes) {
  auto tops = createNodesAndVerifyConnection(3, 2, false, 20);
  auto &nodes = tops.second;

  auto node1_addr = addr_t("de2b1203d72d3549ee2f733b00b2789414c7cea5");
  auto node2_addr = addr_t("973ecb1c08c8eb5a7eaa0d3fd3aab7924f2838b0");
  auto node3_addr = addr_t("4fae949ac2b72960fbe857b56532e2d3c8418d5e");

  // create a transaction transfer coins from node1 to node2
  auto coins_value2 = val_t(100);
  auto gas_price = val_t(2);
  auto data = bytes();
  Transaction trx_master_boot_node_to_node2(
      0, coins_value2, gas_price, TEST_TX_GAS_LIMIT, data,
      nodes[0]->getSecretKey(), node2_addr);
  // broadcast trx and insert
  nodes[0]->insertTransaction(trx_master_boot_node_to_node2, false);

  std::cout << "Checking all nodes see transaction from node 1 to node 2..."
            << std::endl;

  bool checkpoint_passed = false;
  for (auto _(0); _ < 120; ++_) {
    checkpoint_passed = true;
    // test timeout is 60 seconds
    for (auto i(0); i < nodes.size(); ++i) {
      if (nodes[i]->getNumTransactionExecuted() == 0) {
        checkpoint_passed = false;
      }
    }
    if (checkpoint_passed) break;
    taraxa::thisThreadSleepForMilliSeconds(500);
  }

  if (checkpoint_passed == false) {
    for (auto i(0); i < nodes.size(); ++i) {
      ASSERT_EQ(nodes[i]->getNumTransactionExecuted(), 1);
    }
  }

  int pbft_chain_size = 1;
  for (auto i(0); i < nodes.size(); ++i) {
    EXPECT_EQ(nodes[i]->getPbftChainSize(), pbft_chain_size);
  }

  for (auto i(0); i < nodes.size(); ++i) {
    std::cout << "Checking account balances on node " << i << " ..."
              << std::endl;
    EXPECT_EQ(nodes[i]->getBalance(node1_addr).first, 9007199254740991 - 100);
    EXPECT_EQ(nodes[i]->getBalance(node2_addr).first, 100);
    EXPECT_EQ(nodes[i]->getBalance(node3_addr).first, 0);
  }

  // create a transaction transfer coins from node1 to node3
  auto coins_value3 = val_t(1000);
  Transaction trx_master_boot_node_to_node3(
      1, coins_value3, gas_price, TEST_TX_GAS_LIMIT, data,
      nodes[0]->getSecretKey(), node3_addr);
  // broadcast trx and insert
  nodes[0]->insertTransaction(trx_master_boot_node_to_node3, false);

  std::cout << "Checking all nodes see transaction from node 1 to node 3..."
            << std::endl;

  checkpoint_passed = false;
  for (auto _(0); _ < 120; ++_) {
    checkpoint_passed = true;
    // test timeout is 60 seconds
    for (auto i(0); i < nodes.size(); ++i) {
      if (nodes[i]->getNumTransactionExecuted() != 2) {
        checkpoint_passed = false;
      }
    }
    if (checkpoint_passed) break;
    taraxa::thisThreadSleepForMilliSeconds(500);
  }

  if (checkpoint_passed == false) {
    for (auto i(0); i < nodes.size(); ++i) {
      ASSERT_EQ(nodes[i]->getNumTransactionExecuted(), 1);
    }
  }

  pbft_chain_size = 2;
  // Vote DAG block
  for (auto _(0); _ < 120; ++_) {
    // test timeout is 60 seconds
    if (nodes[0]->getPbftChainSize() == pbft_chain_size &&
        nodes[1]->getPbftChainSize() == pbft_chain_size &&
        nodes[2]->getPbftChainSize() == pbft_chain_size) {
      break;
    }
    taraxa::thisThreadSleepForMilliSeconds(500);
  }
  for (auto i(0); i < nodes.size(); ++i) {
    EXPECT_EQ(nodes[i]->getPbftChainSize(), pbft_chain_size);
  }

  for (auto i(0); i < nodes.size(); ++i) {
    std::cout << "Checking account balances on node " << i << " ..."
              << std::endl;
    EXPECT_EQ(nodes[i]->getBalance(node1_addr).first, 9007199254740991 - 1100);
    EXPECT_EQ(nodes[i]->getBalance(node2_addr).first, 100);
    EXPECT_EQ(nodes[i]->getBalance(node3_addr).first, 1000);
  }
  std::unordered_set<blk_hash_t> unique_dag_block_hash_set;
  // PBFT second block
  blk_hash_t pbft_second_block_hash =
      nodes[0]->getPbftChain()->getLastPbftBlockHash();
  PbftBlock pbft_second_block =
      nodes[0]->getPbftChain()->getPbftBlockInChain(pbft_second_block_hash);
  vec_blk_t dag_blocks_hash_in_schedule =
      pbft_second_block.getSchedule().dag_blks_order;
  // due to change of trx packing change, a trx can be packed in multiple blocks
  EXPECT_GE(dag_blocks_hash_in_schedule.size(), 1);
  for (auto &dag_block_hash : dag_blocks_hash_in_schedule) {
    ASSERT_FALSE(unique_dag_block_hash_set.count(dag_block_hash));
    unique_dag_block_hash_set.insert(dag_block_hash);
  }
  // PBFT first block hash
  blk_hash_t pbft_first_block_hash = pbft_second_block.getPrevBlockHash();
  // PBFT first block
  PbftBlock pbft_first_block =
      nodes[0]->getPbftChain()->getPbftBlockInChain(pbft_first_block_hash);
  dag_blocks_hash_in_schedule = pbft_first_block.getSchedule().dag_blks_order;
  // due to change of trx packing change, a trx can be packed in multiple blocks
  EXPECT_GE(dag_blocks_hash_in_schedule.size(), 1);
  for (auto &dag_block_hash : dag_blocks_hash_in_schedule) {
    ASSERT_FALSE(unique_dag_block_hash_set.count(dag_block_hash));
    unique_dag_block_hash_set.insert(dag_block_hash);
  }
}

TEST_F(PbftManagerTest, check_committeeSize_less_or_equal_to_activePlayers) {
  // Set committee size to 1, make sure to be committee <= active_players
  check_2tPlus1_validVotingPlayers_activePlayers_threshold(1);
}

TEST_F(PbftManagerTest, check_committeeSize_greater_than_activePlayers) {
  // Set committee size to 6. Since only running 5 nodes, that will make sure
  // committee > active_players always
  check_2tPlus1_validVotingPlayers_activePlayers_threshold(6);
}

}  // namespace taraxa

int main(int argc, char **argv) {
  taraxa::static_init();
  dev::LoggingOptions logOptions;
  logOptions.verbosity = dev::VerbosityError;
  logOptions.includeChannels.push_back("PBFT_CHAIN");
  logOptions.includeChannels.push_back("PBFT_MGR");
  logOptions.includeChannels.push_back("VOTE_MGR");
  logOptions.includeChannels.push_back("SORTI");
  logOptions.includeChannels.push_back("EXETOR");
  logOptions.includeChannels.push_back("BLK_PP");
  logOptions.includeChannels.push_back("FULLND");
  logOptions.includeChannels.push_back("TRXMGR");
  logOptions.includeChannels.push_back("TRXQU");
  logOptions.includeChannels.push_back("TARCAP");
  dev::setupLogging(logOptions);
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
