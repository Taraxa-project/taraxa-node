#include <gtest/gtest.h>

#include "common/lazy.hpp"
#include "common/static_init.hpp"
#include "logger/logger.hpp"
#include "network/network.hpp"
#include "network/tarcap/packets_handlers/vote_packet_handler.hpp"
#include "test_util/node_dag_creation_fixture.hpp"
#include "vdf/sortition.hpp"

namespace taraxa::core_tests {

struct PbftManagerTest : NodesTest {
  val_t gas_price = 0;
  std::vector<std::shared_ptr<FullNode>> nodes;
  std::vector<uint64_t> nonces;

  void makeNodesWithNonces(const std::vector<taraxa::FullNodeConfig> &cfgs) {
    nodes = launch_nodes(cfgs);
    nonces = std::vector<uint64_t>(cfgs.size(), 1);
  }

  SharedTransaction makeTransaction(uint64_t sender_i, const dev::Address &receiver, u256 value) {
    return std::make_shared<Transaction>(nonces[sender_i]++, value, gas_price, TEST_TX_GAS_LIMIT, bytes(),
                                         nodes[sender_i]->getSecretKey(), receiver);
  }

  std::pair<size_t, size_t> calculate_2tPuls1_threshold(size_t committee_size, size_t valid_voting_players) {
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
    return std::make_pair(two_t_plus_one, threshold);
  }

  void check_2tPlus1_validVotingPlayers_activePlayers_threshold(size_t committee_size) {
    auto node_cfgs = make_node_cfgs(5, 1, 5);
    auto node_1_expected_bal = own_effective_genesis_bal(node_cfgs[0]);
    for (auto &cfg : node_cfgs) {
      cfg.genesis.pbft.committee_size = committee_size;
    }
    makeNodesWithNonces(node_cfgs);

    // Even distribute coins from master boot node to other nodes. Since master
    // boot node owns whole coins, the active players should be only master boot
    // node at the moment.
    uint64_t trxs_count = 0;

    {
      const auto min_stake_to_vote = node_cfgs[0].genesis.state.dpos.eligibility_balance_threshold;
      for (size_t i(1); i < node_cfgs.size(); ++i) {
        const auto trx = make_dpos_trx(node_cfgs[i], min_stake_to_vote, nonces[i]++, gas_price);
        std::cout << "Delegating stake of " << min_stake_to_vote << " to node " << i << ", tx hash: " << trx->getHash()
                  << std::endl;
        nodes[0]->getTransactionManager()->insertTransaction(trx);
        trxs_count++;
      }
      EXPECT_HAPPENS({120s, 1s}, [&](auto &ctx) {
        for (auto &node : nodes) {
          if (ctx.fail_if(node->getDB()->getNumTransactionExecuted() != trxs_count)) {
            return;
          }
        }
      });
    }

    // If previous check passed, delegations txs must have been finalized in block -> take any node's chain size and
    // save it as approx. block number, in which delegation txs were included
    size_t delegations_block = nodes[0]->getPbftChain()->getPbftChainSize();
    ASSERT_GE(delegations_block, 0);
    // Block, in which delegations should be already applied (due to delegation delay)
    size_t delegations_applied_block = delegations_block + node_cfgs[0].genesis.state.dpos.delegation_delay;

    std::vector<u256> balances;
    for (size_t i(0); i < nodes.size(); ++i) {
      balances.push_back(std::move(nodes[i]->getFinalChain()->getBalance(nodes[i]->getAddress()).first));
    }

    const auto init_bal = node_1_expected_bal / nodes.size();
    for (size_t i(1); i < nodes.size(); ++i) {
      auto master_boot_node_send_coins = makeTransaction(0, nodes[i]->getAddress(), init_bal);
      node_1_expected_bal -= init_bal;
      // broadcast trx and insert
      nodes[0]->getTransactionManager()->insertTransaction(master_boot_node_send_coins);
      trxs_count++;
    }

    std::cout << "Checking all nodes executed transactions from master boot node" << std::endl;
    EXPECT_HAPPENS({80s, 8s}, [&](auto &ctx) {
      for (size_t i(0); i < nodes.size(); ++i) {
        if (nodes[i]->getDB()->getNumTransactionExecuted() != trxs_count ||
            nodes[i]->getPbftChain()->getPbftChainSize() < delegations_applied_block) {
          std::cout << "node" << i << " executed " << nodes[i]->getDB()->getNumTransactionExecuted()
                    << " transactions, expected " << trxs_count << ", current chain size "
                    << nodes[i]->getPbftChain()->getPbftChainSize() << ", expected at least "
                    << delegations_applied_block << std::endl;
          auto dummy_trx = makeTransaction(0, nodes[0]->getAddress(), 0);
          // broadcast dummy transaction
          nodes[0]->getTransactionManager()->insertTransaction(dummy_trx);
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
        EXPECT_EQ(nodes[i]->getFinalChain()->getBalance(nodes[j]->getAddress()).first, balances[j] + init_bal);
      }
    }

    uint64_t valid_voting_players = 0;
    size_t committee, two_t_plus_one, expected_2tPlus1, expected_threshold;
    for (size_t i(0); i < nodes.size(); ++i) {
      auto pbft_mgr = nodes[i]->getPbftManager();
      const auto chain_size = nodes[i]->getPbftChain()->getPbftChainSize();
      two_t_plus_one = nodes[i]->getVoteManager()->getPbftTwoTPlusOne(chain_size).value();

      committee = pbft_mgr->getPbftCommitteeSize();
      valid_voting_players = pbft_mgr->getCurrentDposTotalVotesCount().value();
      std::cout << "Node" << i << " committee " << committee << ", valid voting players " << valid_voting_players
                << ", 2t+1 " << two_t_plus_one << std::endl;
      EXPECT_EQ(valid_voting_players, nodes.size());
      std::tie(expected_2tPlus1, expected_threshold) = calculate_2tPuls1_threshold(committee, valid_voting_players);
      EXPECT_EQ(two_t_plus_one, expected_2tPlus1);
    }

    const auto send_coins = 1;
    for (size_t i(0); i < nodes.size(); ++i) {
      // Sending coins in Robin Cycle in order to make all nodes to be active
      // players, but not guarantee
      const auto receiver_index = (i + 1) % nodes.size();
      const auto send_coins_in_robin_cycle = makeTransaction(i, nodes[receiver_index]->getAddress(), send_coins);
      // broadcast trx and insert
      nodes[i]->getTransactionManager()->insertTransaction(send_coins_in_robin_cycle);
      trxs_count++;
    }

    std::cout << "Checking all nodes execute transactions from robin cycle" << std::endl;
    EXPECT_HAPPENS({80s, 8s}, [&](auto &ctx) {
      for (size_t i(0); i < nodes.size(); ++i) {
        if (nodes[i]->getDB()->getNumTransactionExecuted() != trxs_count) {
          std::cout << "node" << i << " executed " << nodes[i]->getDB()->getNumTransactionExecuted()
                    << " transactions. Expected " << trxs_count << std::endl;
          auto dummy_trx = makeTransaction(0, nodes[0]->getAddress(), 0);
          // broadcast dummy transaction
          nodes[0]->getTransactionManager()->insertTransaction(dummy_trx);
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
        EXPECT_EQ(nodes[i]->getFinalChain()->getBalance(nodes[j]->getAddress()).first, balances[j] + init_bal);
      }
    }

    for (size_t i(0); i < nodes.size(); ++i) {
      auto pbft_mgr = nodes[i]->getPbftManager();
      const auto chain_size = nodes[i]->getPbftChain()->getPbftChainSize();
      two_t_plus_one = nodes[i]->getVoteManager()->getPbftTwoTPlusOne(chain_size).value();
      committee = pbft_mgr->getPbftCommitteeSize();
      valid_voting_players = pbft_mgr->getCurrentDposTotalVotesCount().value();
      std::cout << "Node" << i << " committee " << committee << ", valid voting players " << valid_voting_players
                << ", 2t+1 " << two_t_plus_one << std::endl;
      EXPECT_EQ(valid_voting_players, nodes.size());
      std::tie(expected_2tPlus1, expected_threshold) = calculate_2tPuls1_threshold(committee, valid_voting_players);
      EXPECT_EQ(two_t_plus_one, expected_2tPlus1);
    }
  }
};

// Test that after some amount of elapsed time will not continue soft voting for same value
TEST_F(PbftManagerTest, terminate_soft_voting_pbft_block) {
  auto node_cfgs = make_node_cfgs(1, 1, 20);
  makeNodesWithNonces(node_cfgs);

  auto pbft_mgr = nodes[0]->getPbftManager();
  auto vote_mgr = nodes[0]->getVoteManager();
  pbft_mgr->stop();
  std::cout << "PBFT manager stopped" << std::endl;

  // Generate bogus votes
  auto stale_block_hash = blk_hash_t("0000000100000000000000000000000000000000000000000000000000000000");
  auto propose_vote = vote_mgr->generateVote(stale_block_hash, PbftVoteTypes::propose_vote, 2, 2, 1);
  propose_vote->calculateWeight(1, 1, 1);
  vote_mgr->addVerifiedVote(propose_vote);

  // uint64_t time_till_stale_ms = 1000;
  // std::cout << "Set max wait for soft voted value to " << time_till_stale_ms << "ms..." << std::endl;
  // pbft_mgr->setMaxWaitForSoftVotedBlock_ms(time_till_stale_ms);
  // pbft_mgr->setMaxWaitForNextVotedBlock_ms(std::numeric_limits<uint64_t>::max());

  auto sleep_time = 1100;
  std::cout << "Sleep " << sleep_time << "ms so that last soft voted value of " << stale_block_hash.abridged()
            << " becomes stale..." << std::endl;
  taraxa::thisThreadSleepForMilliSeconds(sleep_time);

  std::cout << "Initialize PBFT manager at round 2 step 2" << std::endl;
  pbft_mgr->setPbftRound(2);
  pbft_mgr->setPbftStep(2);
  pbft_mgr->resumeSingleState();
  std::cout << "Into cert voted state in round 2..." << std::endl;
  EXPECT_EQ(pbft_mgr->getPbftRound(), 2);
  EXPECT_EQ(pbft_mgr->getPbftStep(), 3);

  std::cout << "Check did not soft vote for stale soft voted value of " << stale_block_hash.abridged() << "..."
            << std::endl;
  bool skipped_soft_voting = true;
  auto votes = vote_mgr->getVerifiedVotes();
  for (const auto &v : votes) {
    if (PbftVoteTypes::soft_vote == v->getType()) {
      if (v->getBlockHash() == stale_block_hash) {
        skipped_soft_voting = false;
      }
      std::cout << "Found soft voted value of " << v->getBlockHash().abridged() << " in round 2" << std::endl;
    }
  }
  EXPECT_EQ(skipped_soft_voting, true);

  auto start_round = pbft_mgr->getPbftRound();
  pbft_mgr->resume();

  std::cout << "Wait ensure node is still advancing in rounds... " << std::endl;
  EXPECT_HAPPENS({60s, 50ms}, [&](auto &ctx) { WAIT_EXPECT_NE(ctx, start_round, pbft_mgr->getPbftRound()) });
}

// Test that after some amount of elapsed time will give up on the next voting value if corresponding DAG blocks can't
// be found

// TODO: Replace with test that we won't soft vote and invalid block...

/*
TEST_F(PbftManagerTest, terminate_bogus_dag_anchor) {
  auto node_cfgs = make_node_cfgs(1, 1, 20);
  makeNodesWithNonces(node_cfgs);

  auto pbft_mgr = nodes[0]->getPbftManager();
  pbft_mgr->stop();
  std::cout << "PBFT manager stopped" << std::endl;

  auto pbft_chain = nodes[0]->getPbftChain();
  auto vote_mgr = nodes[0]->getVoteManager();

  // Generate bogus DAG anchor for PBFT block
  auto dag_anchor = blk_hash_t("1234567890000000000000000000000000000000000000000000000000000000");
  auto last_pbft_block_hash = pbft_chain->getLastPbftBlockHash();
  auto propose_pbft_period = pbft_chain->getPbftChainSize() + 1;
  auto reward_votes = nodes[0]->getDB()->getLastBlockCertVotes();
  std::vector<vote_hash_t> reward_votes_hashes;
  std::transform(reward_votes.begin(), reward_votes.end(), std::back_inserter(reward_votes_hashes),
                 [](const auto &v) { return v->getHash(); });
  auto beneficiary = nodes[0]->getAddress();
  auto node_sk = nodes[0]->getSecretKey();
  auto propose_pbft_block =
      std::make_shared<PbftBlock>(last_pbft_block_hash, dag_anchor, kNullBlockHash, propose_pbft_period, beneficiary,
                                  node_sk, std::move(reward_votes_hashes));
  auto pbft_block_hash = propose_pbft_block->getBlockHash();
  pbft_chain->pushUnverifiedPbftBlock(propose_pbft_block);

  // Generate bogus vote
  auto period = 1;
  auto round = 1;
  auto step = 4;
  auto propose_vote = pbft_mgr->generateVote(pbft_block_hash, PbftVoteTypes::next_vote, period, round, step);
  propose_vote->calculateWeight(1, 1, 1);
  vote_mgr->addVerifiedVote(propose_vote);

  std::cout << "Initialize PBFT manager at round 1 step 4" << std::endl;
  pbft_mgr->setPbftRound(1);
  pbft_mgr->setPbftStep(4);
  pbft_mgr->start();

  // Vote at the bogus PBFT block hash
  EXPECT_HAPPENS({10s, 50ms}, [&](auto &ctx) {
    blk_hash_t soft_vote_value;
    auto votes = vote_mgr->getVerifiedVotes();
    for (const auto &v : votes) {
      if (PbftVoteTypes::soft_vote == v->getType() && v->getBlockHash() == pbft_block_hash) {
        soft_vote_value = v->getBlockHash();
        break;
      }
    }

    WAIT_EXPECT_EQ(ctx, soft_vote_value, pbft_block_hash)
  });

  std::cout << "After some time, terminate voting on the bogus value " << pbft_block_hash << std::endl;
  EXPECT_HAPPENS({10s, 50ms}, [&](auto &ctx) {
    auto proposal_value = pbft_block_hash;
    auto votes = vote_mgr->getVerifiedVotes();
    for (const auto &v : votes) {
      if (PbftVoteTypes::propose_vote == v->getType() && v->getBlockHash() != pbft_block_hash) {
        proposal_value = v->getBlockHash();
        break;
      }
    }

    WAIT_EXPECT_NE(ctx, proposal_value, pbft_block_hash)
  });

  std::cout << "Wait ensure node is still advancing in rounds... " << std::endl;
  auto start_round = pbft_mgr->getPbftRound();
  EXPECT_HAPPENS({60s, 50ms}, [&](auto &ctx) { WAIT_EXPECT_NE(ctx, start_round, pbft_mgr->getPbftRound()) });
}

// Test that after some number of rounds will give up the proposing value if proposed block is not available
TEST_F(PbftManagerTest, terminate_missing_proposed_pbft_block) {
  auto node_cfgs = make_node_cfgs(1, 1, 20);
  makeNodesWithNonces(node_cfgs);

  auto pbft_mgr = nodes[0]->getPbftManager();
  pbft_mgr->stop();
  std::cout << "Initialize PBFT manager at round 1 step 4" << std::endl;

  auto pbft_chain = nodes[0]->getPbftChain();
  auto vote_mgr = nodes[0]->getVoteManager();
  auto node_sk = nodes[0]->getSecretKey();

  // Generate bogus vote
  auto period = 1;
  auto round = 1;
  auto step = 4;
  auto pbft_block_hash = blk_hash_t("0000000100000000000000000000000000000000000000000000000000000000");
  auto next_vote = pbft_mgr->generateVote(pbft_block_hash, PbftVoteTypes::next_vote, period, round, step);
  next_vote->calculateWeight(1, 1, 1);
  vote_mgr->addVerifiedVote(next_vote);

  std::cout << "Initialize PBFT manager at round " << round << " step " << step << std::endl;
  pbft_mgr->setPbftRound(round);
  pbft_mgr->setPbftStep(step);
  pbft_mgr->start();

  // Vote at the bogus PBFT block hash
  EXPECT_HAPPENS({10s, 50ms}, [&](auto &ctx) {
    blk_hash_t soft_vote_value;
    auto votes = vote_mgr->getVerifiedVotes();
    for (auto const &v : votes) {
      if (PbftVoteTypes::soft_vote == v->getType() && v->getBlockHash() == pbft_block_hash) {
        soft_vote_value = v->getBlockHash();
        break;
      }
    }

    WAIT_EXPECT_EQ(ctx, soft_vote_value, pbft_block_hash)
  });

  std::cout << "After some time, terminate voting on the missing proposed block " << pbft_block_hash << std::endl;
  // After some rounds, terminate the bogus PBFT block value and propose PBFT block with NULL anchor
  EXPECT_HAPPENS({20s, 50ms}, [&](auto &ctx) {
    auto proposal_value = pbft_block_hash;
    auto votes = vote_mgr->getVerifiedVotes();

    for (auto const &v : votes) {
      if (PbftVoteTypes::propose_vote == v->getType() && v->getBlockHash() != pbft_block_hash) {
        // PBFT has terminated on the missing PBFT block value and proposed a new block value
        proposal_value = v->getBlockHash();
        break;
      }
    }

    WAIT_EXPECT_NE(ctx, proposal_value, pbft_block_hash)
  });

  std::cout << "Wait ensure node is still advancing in rounds... " << std::endl;
  auto start_round = pbft_mgr->getPbftRound();
  EXPECT_HAPPENS({60s, 50ms}, [&](auto &ctx) { WAIT_EXPECT_NE(ctx, start_round, pbft_mgr->getPbftRound()) });
}
*/

TEST_F(PbftManagerTest, full_node_lambda_input_test) {
  auto node = create_nodes(1, true).front();
  node->start();

  auto pbft_mgr = node->getPbftManager();
  EXPECT_EQ(pbft_mgr->getPbftInitialLambda().count(), 2000);
}

TEST_F(PbftManagerTest, check_get_eligible_vote_count) {
  auto node_cfgs = make_node_cfgs(5, 1, 5);
  auto node_1_expected_bal = own_effective_genesis_bal(node_cfgs[0]);
  for (auto &cfg : node_cfgs) {
    cfg.genesis.pbft.committee_size = 100;
  }
  makeNodesWithNonces(node_cfgs);

  // Even distribute coins from master boot node to other nodes. Since master
  // boot node owns whole coins, the active players should be only master boot
  // node at the moment.
  uint64_t trxs_count = 0;
  auto expected_eligible_total_vote = 1;
  {
    const auto min_stake_to_vote = node_cfgs[0].genesis.state.dpos.eligibility_balance_threshold;
    for (size_t i(1); i < node_cfgs.size(); ++i) {
      std::cout << "Delegating stake of " << min_stake_to_vote << " to node " << i << std::endl;
      const auto trx = make_dpos_trx(node_cfgs[i], min_stake_to_vote, nonces[i]++, gas_price);
      nodes[0]->getTransactionManager()->insertTransaction(trx);
      trxs_count++;
      expected_eligible_total_vote++;
    }
    EXPECT_HAPPENS({120s, 1s}, [&](auto &ctx) {
      for (auto &node : nodes) {
        if (ctx.fail_if(node->getDB()->getNumTransactionExecuted() != trxs_count)) {
          return;
        }
      }
    });
  }

  std::vector<u256> balances;
  for (size_t i(0); i < nodes.size(); ++i) {
    balances.push_back(std::move(nodes[i]->getFinalChain()->getBalance(nodes[i]->getAddress()).first));
  }

  const auto init_bal = node_1_expected_bal / nodes.size() / 2;
  for (size_t i(1); i < nodes.size(); ++i) {
    auto master_boot_node_send_coins = makeTransaction(0, nodes[i]->getAddress(), init_bal);
    node_1_expected_bal -= init_bal;
    // broadcast trx and insert
    nodes[0]->getTransactionManager()->insertTransaction(master_boot_node_send_coins);
    trxs_count++;
  }

  std::cout << "Checking all nodes executed transactions from master boot node" << std::endl;
  EXPECT_HAPPENS({80s, 8s}, [&](auto &ctx) {
    for (size_t i(0); i < nodes.size(); ++i) {
      if (nodes[i]->getDB()->getNumTransactionExecuted() != trxs_count) {
        std::cout << "node" << i << " executed " << nodes[i]->getDB()->getNumTransactionExecuted()
                  << " transactions, expected " << trxs_count << std::endl;
        auto dummy_trx = makeTransaction(0, nodes[0]->getAddress(), 0);
        // broadcast dummy transaction
        nodes[0]->getTransactionManager()->insertTransaction(dummy_trx);
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
      EXPECT_EQ(nodes[i]->getFinalChain()->getBalance(nodes[j]->getAddress()).first, balances[j] + init_bal);
    }
  }

  const auto send_coins = 1;
  for (size_t i(0); i < nodes.size(); ++i) {
    // Sending coins in Robin Cycle in order to make all nodes to be active
    // players, but not guarantee
    const auto receiver_index = (i + 1) % nodes.size();
    const auto send_coins_in_robin_cycle = makeTransaction(i, nodes[receiver_index]->getAddress(), send_coins);
    // broadcast trx and insert
    nodes[i]->getTransactionManager()->insertTransaction(send_coins_in_robin_cycle);
    trxs_count++;
  }

  std::cout << "Checking all nodes execute transactions from robin cycle" << std::endl;
  EXPECT_HAPPENS({20s, 4s}, [&](auto &ctx) {
    for (size_t i(0); i < nodes.size(); ++i) {
      if (nodes[i]->getDB()->getNumTransactionExecuted() != trxs_count) {
        std::cout << "node" << i << " executed " << nodes[i]->getDB()->getNumTransactionExecuted()
                  << " transactions. Expected " << trxs_count << std::endl;
        auto dummy_trx = makeTransaction(0, nodes[0]->getAddress(), 0);
        // broadcast dummy transaction
        nodes[0]->getTransactionManager()->insertTransaction(dummy_trx);
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
      EXPECT_EQ(nodes[i]->getFinalChain()->getBalance(nodes[j]->getAddress()).first, balances[j] + init_bal);
    }
  }

  uint64_t eligible_total_vote_count = 0;
  size_t committee, two_t_plus_one, expected_2tPlus1, expected_threshold;
  for (size_t i(0); i < nodes.size(); ++i) {
    auto pbft_mgr = nodes[i]->getPbftManager();
    const auto chain_size = nodes[i]->getPbftChain()->getPbftChainSize();
    two_t_plus_one = nodes[i]->getVoteManager()->getPbftTwoTPlusOne(chain_size).value();
    committee = pbft_mgr->getPbftCommitteeSize();
    eligible_total_vote_count = pbft_mgr->getCurrentDposTotalVotesCount().value();
    std::cout << "Node" << i << " committee " << committee << ", eligible total vote count "
              << eligible_total_vote_count << ", 2t+1 " << two_t_plus_one << std::endl;
    EXPECT_EQ(eligible_total_vote_count, expected_eligible_total_vote);
    std::tie(expected_2tPlus1, expected_threshold) = calculate_2tPuls1_threshold(committee, eligible_total_vote_count);
    EXPECT_EQ(two_t_plus_one, expected_2tPlus1);
  }
}

TEST_F(PbftManagerTest, pbft_produce_blocks_with_null_anchor) {
  auto node_cfgs = make_node_cfgs(1, 1, 20);
  auto node = create_nodes(node_cfgs, true).front();
  EXPECT_EQ(own_balance(node), own_effective_genesis_bal(node_cfgs[0]));

  // Check PBFT produced blocks with no transactions
  auto pbft_chain = node->getPbftChain();
  EXPECT_HAPPENS({10s, 200ms}, [&](auto &ctx) { WAIT_EXPECT_GT(ctx, pbft_chain->getPbftChainSize(), 1) });
}

TEST_F(PbftManagerTest, pbft_manager_run_single_node) {
  auto node_cfgs = make_node_cfgs(1, 1, 20);
  makeNodesWithNonces(node_cfgs);
  auto node = nodes.front();

  auto receiver = addr_t("973ecb1c08c8eb5a7eaa0d3fd3aab7924f2838b0");
  EXPECT_EQ(own_balance(node), own_effective_genesis_bal(node_cfgs[0]));
  const auto old_balance = node->getFinalChain()->getBalance(receiver).first;

  // create a transaction
  const auto coins_value = val_t(100);
  const auto data = bytes();
  auto trx_master_boot_node_to_receiver = makeTransaction(0, receiver, coins_value);
  node->getTransactionManager()->insertTransaction(trx_master_boot_node_to_receiver);

  // Check there is proposing DAG blocks
  EXPECT_HAPPENS({1s, 200ms}, [&](auto &ctx) {
    WAIT_EXPECT_EQ(ctx, node->getPbftChain()->getPbftChainSizeExcludingEmptyPbftBlocks(), 1)
  });

  // Make sure the transaction get executed
  EXPECT_HAPPENS({1s, 200ms}, [&](auto &ctx) { WAIT_EXPECT_EQ(ctx, node->getDB()->getNumTransactionExecuted(), 1) });

  EXPECT_EQ(own_balance(node), own_effective_genesis_bal(node_cfgs[0]) - coins_value);
  EXPECT_EQ(node->getFinalChain()->getBalance(receiver).first, old_balance + coins_value);
}

TEST_F(PbftManagerTest, pbft_manager_run_multi_nodes) {
  const auto node_cfgs = make_node_cfgs(3, 1, 20);
  const auto node1_genesis_bal = own_effective_genesis_bal(node_cfgs[0]);
  const auto node2_genesis_bal = own_effective_genesis_bal(node_cfgs[1]);
  const auto node3_genesis_bal = own_effective_genesis_bal(node_cfgs[2]);
  makeNodesWithNonces(node_cfgs);

  const auto node1_addr = nodes[0]->getAddress();
  const auto node2_addr = nodes[1]->getAddress();
  const auto node3_addr = nodes[2]->getAddress();

  // create a transaction transfer coins from node1 to node2
  auto trx_master_boot_node_to_node2 = makeTransaction(0, node2_addr, 100);
  // broadcast trx and insert
  nodes[0]->getTransactionManager()->insertTransaction(trx_master_boot_node_to_node2);

  // Only node1 be able to propose DAG block
  EXPECT_HAPPENS({5s, 200ms}, [&](auto &ctx) {
    WAIT_EXPECT_EQ(ctx, nodes[0]->getPbftChain()->getPbftChainSizeExcludingEmptyPbftBlocks(), 1)
  });

  const expected_balances_map_t expected_balances1 = {
      {node1_addr, node1_genesis_bal - 100}, {node2_addr, node2_genesis_bal + 100}, {node3_addr, node3_genesis_bal}};
  wait_for_balances(nodes, expected_balances1, {100s, 500ms});

  // create a transaction transfer coins from node1 to node3
  auto trx_master_boot_node_to_node3 = makeTransaction(0, node3_addr, 1000);
  // broadcast trx and insert
  nodes[0]->getTransactionManager()->insertTransaction(trx_master_boot_node_to_node3);

  // Only node1 be able to propose DAG block
  EXPECT_HAPPENS({5s, 200ms}, [&](auto &ctx) {
    WAIT_EXPECT_EQ(ctx, nodes[0]->getPbftChain()->getPbftChainSizeExcludingEmptyPbftBlocks(), 2)
  });

  std::cout << "Checking all nodes see transaction from node 1 to node 3..." << std::endl;
  const expected_balances_map_t expected_balances2 = {{node1_addr, node1_genesis_bal - 1100},
                                                      {node2_addr, node2_genesis_bal + 100},
                                                      {node3_addr, node3_genesis_bal + 1000}};
  wait_for_balances(nodes, expected_balances2, {100s, 500ms});
}

TEST_F(PbftManagerTest, propose_block_and_vote_broadcast) {
  auto node_cfgs = make_node_cfgs(3);

  makeNodesWithNonces(node_cfgs);
  auto &node1 = nodes[0];
  auto &node2 = nodes[1];
  auto &node3 = nodes[2];

  // Stop PBFT manager and executor to test networking
  node1->getPbftManager()->stop();
  node2->getPbftManager()->stop();
  node3->getPbftManager()->stop();

  auto pbft_mgr1 = node1->getPbftManager();
  auto pbft_mgr2 = node2->getPbftManager();
  auto pbft_mgr3 = node3->getPbftManager();

  auto nw1 = node1->getNetwork();
  auto nw2 = node2->getNetwork();
  auto nw3 = node3->getNetwork();

  // Check all 3 nodes PBFT chain synced
  EXPECT_HAPPENS({300s, 200ms}, [&](auto &ctx) {
    WAIT_EXPECT_EQ(ctx, pbft_mgr1->pbftSyncingPeriod(), pbft_mgr2->pbftSyncingPeriod())
    WAIT_EXPECT_EQ(ctx, pbft_mgr1->pbftSyncingPeriod(), pbft_mgr3->pbftSyncingPeriod())
    WAIT_EXPECT_EQ(ctx, pbft_mgr2->pbftSyncingPeriod(), pbft_mgr3->pbftSyncingPeriod())
  });

  blk_hash_t prev_block_hash = node1->getPbftChain()->getLastPbftBlockHash();

  // Node1 generate a PBFT block sample
  auto reward_votes = node1->getDB()->getLastBlockCertVotes();
  std::vector<vote_hash_t> reward_votes_hashes;
  std::transform(reward_votes.begin(), reward_votes.end(), std::back_inserter(reward_votes_hashes),
                 [](const auto &v) { return v->getHash(); });
  auto proposed_pbft_block = std::make_shared<PbftBlock>(
      prev_block_hash, kNullBlockHash, kNullBlockHash, kNullBlockHash, node1->getPbftManager()->getPbftPeriod(),
      node1->getAddress(), node1->getSecretKey(), std::move(reward_votes_hashes));
  auto propose_vote = node1->getVoteManager()->generateVote(
      proposed_pbft_block->getBlockHash(), PbftVoteTypes::propose_vote, proposed_pbft_block->getPeriod(),
      node1->getPbftManager()->getPbftRound() + 1, value_proposal_state);
  pbft_mgr1->processProposedBlock(proposed_pbft_block, propose_vote);

  auto block1_from_node1 = pbft_mgr1->getProposedBlocksSt().getPbftProposedBlock(
      propose_vote->getPeriod(), propose_vote->getRound(), propose_vote->getBlockHash());
  ASSERT_TRUE(block1_from_node1);
  EXPECT_EQ(block1_from_node1->first->getJsonStr(), proposed_pbft_block->getJsonStr());

  nw1->getSpecificHandler<network::tarcap::VotePacketHandler>()->onNewPbftVote(propose_vote, proposed_pbft_block);

  // Check node2 and node3 receive the PBFT block
  std::optional<std::pair<std::shared_ptr<PbftBlock>, bool>> node2_synced_proposed_block_and_vote = std::nullopt;
  std::optional<std::pair<std::shared_ptr<PbftBlock>, bool>> node3_synced_proposed_block_and_vote = std::nullopt;

  EXPECT_HAPPENS({20s, 200ms}, [&](auto &ctx) {
    node2_synced_proposed_block_and_vote = pbft_mgr2->getProposedBlocksSt().getPbftProposedBlock(
        propose_vote->getPeriod(), propose_vote->getRound(), propose_vote->getBlockHash());
    node3_synced_proposed_block_and_vote = pbft_mgr3->getProposedBlocksSt().getPbftProposedBlock(
        propose_vote->getPeriod(), propose_vote->getRound(), propose_vote->getBlockHash());

    WAIT_EXPECT_TRUE(ctx, node2_synced_proposed_block_and_vote)
    WAIT_EXPECT_TRUE(ctx, node3_synced_proposed_block_and_vote)
  });
  EXPECT_EQ(node2_synced_proposed_block_and_vote->first->getJsonStr(), proposed_pbft_block->getJsonStr());
  EXPECT_EQ(node3_synced_proposed_block_and_vote->first->getJsonStr(), proposed_pbft_block->getJsonStr());
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

struct PbftManagerWithDagCreation : NodeDagCreationFixture {};

TEST_F(PbftManagerWithDagCreation, trx_generation) {
  makeNode();
  deployContract();

  auto testing_cycle_txs_count = {10, 100, 1000};
  for (const auto &count : testing_cycle_txs_count) {
    auto txs_before = nonce;
    auto trxs = makeTransactions(count);
    EXPECT_EQ(trxs.size(), count);
    EXPECT_EQ(trxs.front()->getNonce(), txs_before);
    EXPECT_EQ(trxs.back()->getNonce(), txs_before + trxs.size() - 1);
    insertTransactions(trxs);

    EXPECT_HAPPENS({10s, 500ms},
                   [&](auto &ctx) { WAIT_EXPECT_EQ(ctx, node->getDB()->getNumTransactionExecuted(), nonce - 1); });
    std::cout << "Creation and applying of " << count << " transactions is ok" << std::endl;
  }
}

TEST_F(PbftManagerWithDagCreation, initial_dag) {
  makeNode();
  deployContract();

  auto prev_value = node->getDagManager()->getNumVerticesInDag().first;
  generateAndApplyInitialDag();

  EXPECT_HAPPENS({10s, 250ms}, [&](auto &ctx) {
    WAIT_EXPECT_EQ(ctx, node->getDagManager()->getNumVerticesInDag().second, prev_value + getInitialDagSize());
  });
}

TEST_F(PbftManagerWithDagCreation, dag_generation) {
  makeNode();
  deployContract();
  node->getDagBlockProposer()->stop();
  generateAndApplyInitialDag();

  EXPECT_HAPPENS({10s, 250ms}, [&](auto &ctx) {
    WAIT_EXPECT_EQ(ctx, node->getFinalChain()->get_account(node->getAddress())->nonce, nonce);
  });

  auto nonce_before = nonce;
  {
    auto blocks = generateDagBlocks(20, 5, 5);
    insertBlocks(std::move(blocks));
  }

  auto tx_count = 20 * 5 * 5 + 1;
  EXPECT_EQ(nonce, nonce_before + tx_count);

  EXPECT_HAPPENS({60s, 250ms}, [&](auto &ctx) {
    WAIT_EXPECT_EQ(ctx, node->getFinalChain()->get_account(node->getAddress())->nonce, nonce);
    WAIT_EXPECT_EQ(ctx, node->getDB()->getNumTransactionExecuted(), nonce - 1);
  });
}

TEST_F(PbftManagerWithDagCreation, limit_dag_block_size) {
  auto node_cfgs = make_node_cfgs(1, 1, 5, true);
  node_cfgs.front().genesis.dag.gas_limit = 500000;
  makeNodeFromConfig(node_cfgs);
  deployContract();

  auto greet = [&] {
    auto ret = node->getFinalChain()->call({
        node->getAddress(),
        0,
        contract_addr,
        0,
        0,
        TEST_TX_GAS_LIMIT,
        // greet()
        dev::fromHex("0xcfae3217"),
    });
    return dev::toHexPrefixed(ret.code_retval);
  };
  ASSERT_EQ(
      greet(),
      // "Hello"
      "0x0000000000000000000000000000000000000000000000000000000000000020000000000000000000000000000000000000000000000"
      "000000000000000000548656c6c6f000000000000000000000000000000000000000000000000000000");

  generateAndApplyInitialDag();

  auto trxs_before = node->getTransactionManager()->getTransactionCount();
  EXPECT_HAPPENS({10s, 500ms},
                 [&](auto &ctx) { WAIT_EXPECT_EQ(ctx, trxs_before, node->getDB()->getNumTransactionExecuted()); });

  auto pivot_before = node->getDagManager()->getDagFrontier().pivot;

  const uint32_t additional_trx_count = 30;
  insertTransactions(makeTransactions(additional_trx_count));

  uint64_t should_be_in_one_dag_block = node->getConfig().genesis.dag.gas_limit / trxEstimation();
  EXPECT_HAPPENS({10s, 250ms}, [&](auto &ctx) {
    WAIT_EXPECT_EQ(ctx, node->getDB()->getNumTransactionExecuted(), trxs_before + additional_trx_count)
    WAIT_EXPECT_EQ(ctx, node->getTransactionManager()->getTransactionCount(), trxs_before + additional_trx_count)
  });

  auto dag_block = node->getDB()->getDagBlock(node->getDagManager()->getDagFrontier().pivot);
  while (dag_block->getPivot() != pivot_before) {
    dag_block = node->getDB()->getDagBlock(dag_block->getPivot());
  }

  ASSERT_EQ(should_be_in_one_dag_block, dag_block->getTrxs().size());

  ASSERT_EQ(greet(),
            // "Hola"
            "0x000000000000000000000000000000000000000000000000000000000000002000"
            "00000000000000000000000000000000000000000000000000000000000004486f"
            "6c6100000000000000000000000000000000000000000000000000000000");
}

TEST_F(PbftManagerWithDagCreation, limit_pbft_block) {
  auto node_cfgs = make_node_cfgs(1, 1, 5, true);
  node_cfgs.front().genesis.dag.gas_limit = 500000;
  node_cfgs.front().genesis.pbft.gas_limit = 1100000;
  makeNodeFromConfig(node_cfgs);

  deployContract();
  node->getDagBlockProposer()->stop();
  generateAndApplyInitialDag();

  auto trxs_before = node->getTransactionManager()->getTransactionCount();
  EXPECT_HAPPENS({10s, 500ms},
                 [&](auto &ctx) { WAIT_EXPECT_EQ(ctx, trxs_before, node->getDB()->getNumTransactionExecuted()); });

  auto starting_block_number = node->getFinalChain()->last_block_number();
  auto trx_in_block = 5;
  insertBlocks(generateDagBlocks(20, 5, trx_in_block));

  uint64_t tx_count = 20 * 5 * 5 + 1;

  EXPECT_HAPPENS({60s, 500ms}, [&](auto &ctx) {
    WAIT_EXPECT_EQ(ctx, node->getDB()->getNumTransactionExecuted(), trxs_before + tx_count);
  });

  auto max_pbft_block_capacity = node_cfgs.front().genesis.pbft.gas_limit / (trxEstimation() * 5);
  for (size_t i = starting_block_number; i < node->getFinalChain()->last_block_number(); ++i) {
    const auto &blk_hash = node->getDB()->getPeriodBlockHash(i);
    ASSERT_TRUE(blk_hash != kNullBlockHash);
    const auto &pbft_block = node->getPbftChain()->getPbftBlockInChain(blk_hash);
    const auto &dag_blocks_order = node->getDagManager()->getDagBlockOrder(pbft_block.getPivotDagBlockHash(), i);

    EXPECT_LE(dag_blocks_order.size(), max_pbft_block_capacity);
  }
}

TEST_F(PbftManagerWithDagCreation, produce_overweighted_block) {
  auto node_cfgs = make_node_cfgs(1, 1, 5, true);
  auto dag_gas_limit = node_cfgs.front().genesis.dag.gas_limit = 500000;
  node_cfgs.front().genesis.pbft.gas_limit = 1100000;
  makeNodeFromConfig(node_cfgs);

  deployContract();
  node->getDagBlockProposer()->stop();
  generateAndApplyInitialDag();

  auto trx_count = node->getTransactionManager()->getTransactionCount();
  EXPECT_HAPPENS({10s, 500ms},
                 [&](auto &ctx) { WAIT_EXPECT_EQ(ctx, trx_count, node->getDB()->getNumTransactionExecuted()); });

  auto starting_block_number = node->getFinalChain()->last_block_number();
  const auto trx_in_block = dag_gas_limit / trxEstimation() + 2;
  insertBlocks(generateDagBlocks(1, 5, trx_in_block));

  // We need to move one block forward when we will start applying those generated DAGs and transactions
  EXPECT_HAPPENS({10s, 100ms}, [&](auto &ctx) {
    WAIT_EXPECT_EQ(ctx, node->getFinalChain()->last_block_number(), starting_block_number + 1);
  });
  // check that new created transaction wasn't executed in that previous block
  ASSERT_EQ(trx_count, node->getDB()->getNumTransactionExecuted());
  ++starting_block_number;

  trx_count += 5 * trx_in_block + 1;
  // We are starting to process new dag blocks only from the next period(block), so add 1
  EXPECT_HAPPENS({10s, 100ms}, [&](auto &ctx) {
    // all transactions should be included in 2 blocks
    WAIT_EXPECT_EQ(ctx, node->getDB()->getNumTransactionExecuted(), trx_count);
    WAIT_EXPECT_EQ(ctx, node->getFinalChain()->last_block_number(), starting_block_number + 2);
  });

  // verify that last block is overweighted, but it is in chain
  const auto period = node->getFinalChain()->last_block_number();
  auto period_raw = node->getDB()->getPeriodDataRaw(period);
  ASSERT_FALSE(period_raw.empty());
  PeriodData period_data(period_raw);
  EXPECT_FALSE(node->getPbftManager()->checkBlockWeight(period_data.dag_blocks));
}

TEST_F(PbftManagerWithDagCreation, DISABLED_pbft_block_is_overweighted) {
  auto node_cfgs = make_node_cfgs(1, 5, true);
  node_cfgs.front().genesis.dag.gas_limit = 500000;
  node_cfgs.front().genesis.pbft.gas_limit = 600000;
  makeNode();
  deployContract();
  node->getDagBlockProposer()->stop();
  generateAndApplyInitialDag();

  EXPECT_HAPPENS({10s, 500ms},
                 [&](auto &ctx) { WAIT_EXPECT_EQ(ctx, nonce, node->getDB()->getNumTransactionExecuted() + 1); });

  node->getPbftManager()->stop();
  // create pbft block
  auto chain_size_before = node->getPbftChain()->getPbftChainSize();
  {
    auto blocks_with_txs = generateDagBlocks(10, 3, 1);
    insertBlocks(blocks_with_txs);
    auto dag_block_hash = blocks_with_txs.back().blk.getHash();

    // get DAG block and transaction order
    const auto propose_period = node->getPbftChain()->getPbftChainSize() + 1;
    auto dag_block_order = node->getDagManager()->getDagBlockOrder(dag_block_hash, propose_period);
    ASSERT_TRUE(!dag_block_order.empty());

    std::vector<trx_hash_t> trx_hashes;
    for (const auto &bt : blocks_with_txs) {
      std::transform(bt.trxs.begin(), bt.trxs.end(), std::back_inserter(trx_hashes),
                     [](const auto &t) { return t->getHash(); });
    }
    auto order_hash = node->getPbftManager()->calculateOrderHash(dag_block_order);

    const auto &last_hash = node->getPbftChain()->getLastPbftBlockHash();
    auto reward_votes = node->getDB()->getLastBlockCertVotes();
    std::vector<vote_hash_t> reward_votes_hashes;
    std::transform(reward_votes.begin(), reward_votes.end(), std::back_inserter(reward_votes_hashes),
                   [](const auto &v) { return v->getHash(); });
    const auto pbft_block =
        std::make_shared<PbftBlock>(last_hash, dag_block_hash, order_hash, kNullBlockHash, propose_period,
                                    node->getAddress(), node->getSecretKey(), std::move(reward_votes_hashes));
    // node->getPbftChain()->pushUnverifiedPbftBlock(pbft_block);
  }

  EXPECT_HAPPENS({60s, 500ms}, [&](auto &ctx) {
    WAIT_EXPECT_EQ(ctx, node->getPbftChain()->getPbftChainSize(), chain_size_before + 1);
  });
}

TEST_F(PbftManagerWithDagCreation, proposed_blocks) {
  auto db = std::make_shared<DbStorage>(data_dir);
  ProposedBlocks proposed_blocks(db);

  std::map<blk_hash_t, std::shared_ptr<PbftBlock>> blocks;
  const uint32_t block_count = 100;
  // Create blocks
  for (uint32_t i = 1; i <= block_count; i++) {
    std::vector<vote_hash_t> reward_votes_hashes;
    auto block = std::make_shared<PbftBlock>(blk_hash_t(1), kNullBlockHash, kNullBlockHash, kNullBlockHash, 2, addr_t(),
                                             dev::KeyPair::create().secret(), std::move(reward_votes_hashes));
    blocks.insert({block->getBlockHash(), block});
  }
  auto now = std::chrono::steady_clock::now();
  for (auto b : blocks) {
    proposed_blocks.pushProposedPbftBlock(1, b.second);
  }
  std::cout << "Time to push " << block_count
            << " blocks: " << duration_cast<microseconds>(std::chrono::steady_clock::now() - now).count()
            << " microseconds" << std::endl;
  auto blocks_from_db = db->getProposedPbftBlocks();
  EXPECT_EQ(blocks_from_db.size(), blocks.size());
  for (auto b : blocks_from_db) {
    EXPECT_TRUE(blocks.find(b.second->getBlockHash()) != blocks.end());
  }
  now = std::chrono::steady_clock::now();
  proposed_blocks.cleanupProposedPbftBlocksByPeriod(3);
  std::cout << "Time to erase " << block_count
            << " blocks: " << duration_cast<microseconds>(std::chrono::steady_clock::now() - now).count()
            << " microseconds" << std::endl;
  blocks_from_db = db->getProposedPbftBlocks();
  EXPECT_EQ(blocks_from_db.size(), 0);
}

TEST_F(PbftManagerWithDagCreation, state_root_hash) {
  makeNode();
  deployContract();
  node->getDagBlockProposer()->stop();
  const auto lambda = node->getConfig().genesis.pbft.lambda_ms;
  auto prev_value = node->getDagManager()->getNumVerticesInDag().first;
  generateAndApplyInitialDag();
  EXPECT_HAPPENS({10s, 250ms}, [&](auto &ctx) {
    WAIT_EXPECT_EQ(ctx, node->getDagManager()->getNumVerticesInDag().second, prev_value + getInitialDagSize());
  });
  // generate dag blocks with delays to distribute them between pbft blocks
  for (uint8_t i = 0; i < 5; ++i) {
    auto blocks = generateDagBlocks(2, 2, 2);
    insertBlocks(std::move(blocks));
    std::this_thread::sleep_for(std::chrono::milliseconds(3 * lambda));
  }

  EXPECT_HAPPENS({5s, 500ms}, [&](auto &ctx) {
    WAIT_EXPECT_EQ(ctx, node->getFinalChain()->get_account(node->getAddress())->nonce, nonce);
    WAIT_EXPECT_EQ(ctx, node->getDB()->getNumTransactionExecuted(), nonce - 1);
  });

  const auto &state_root_delay = node_cfgs.front().genesis.pbft.state_root_recording_delay;
  const auto &head_hash = node->getPbftChain()->getLastPbftBlockHash();
  auto pbft_block = node->getPbftChain()->getPbftBlockInChain(head_hash);
  // Check that all produced blocks have correct state_root_hashes
  while (pbft_block.getPeriod() != 1) {
    auto period = pbft_block.getPeriod();
    h256 state_root;
    if (period > state_root_delay) {
      state_root = node->getFinalChain()->block_header(period - state_root_delay)->state_root;
    }
    EXPECT_EQ(pbft_block.getPrevStateRoot(), state_root);

    pbft_block = node->getPbftChain()->getPbftBlockInChain(pbft_block.getPrevBlockHash());
  }
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
