#include "pbft/pbft_manager.hpp"

#include <gtest/gtest.h>

#include "common/lazy.hpp"
#include "common/static_init.hpp"
#include "logger/log.hpp"
#include "network/network.hpp"
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
  auto node_cfgs = make_node_cfgs<5>(5);
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
      std::cout << "Delegating stake of " << min_stake_to_vote << " to node " << i << std::endl;
      node_1_expected_bal -= delegations[nodes[i]->getAddress()].value = min_stake_to_vote;
    }
    auto trx = make_dpos_trx(node_cfgs[0], delegations, nonce++);
    nodes[0]->getTransactionManager()->insertTransaction(trx, false);
    trxs_count++;
    EXPECT_HAPPENS({120s, 1s}, [&](auto &ctx) {
      for (auto &node : nodes) {
        if (ctx.fail_if(!node->getFinalChain()->transaction_location(trx.getHash()))) {
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
    nodes[0]->getTransactionManager()->insertTransaction(master_boot_node_send_coins, false);
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
        nodes[0]->getTransactionManager()->insertTransaction(dummy_trx, false);
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
    nodes[i]->getTransactionManager()->insertTransaction(send_coins_in_robin_cycle, false);
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

// Test that after some amount of elapsed time will not continue soft voting for same value
TEST_F(PbftManagerTest, terminate_soft_voting_pbft_block) {
  auto node_cfgs = make_node_cfgs<20>(1);
  auto nodes = launch_nodes(node_cfgs);

  auto pbft_mgr = nodes[0]->getPbftManager();
  pbft_mgr->stop();

  cout << "Initialize PBFT manager at round 2 step 2" << endl;

  auto vote_mgr = nodes[0]->getVoteManager();

  // Generate bogus votes
  auto weighted_index = 0;
  auto stale_block_hash = blk_hash_t("0000000100000000000000000000000000000000000000000000000000000000");
  auto alternate_propose_block_hash = blk_hash_t("0000000200000000000000000000000000000000000000000000000000000000");
  auto prev_round_next_vote = pbft_mgr->generateVote(stale_block_hash, next_vote_type, 1, 4, weighted_index);
  vote_mgr->addVerifiedVote(prev_round_next_vote);
  auto propose_vote = pbft_mgr->generateVote(stale_block_hash, propose_vote_type, 2, 1, weighted_index);
  vote_mgr->addVerifiedVote(propose_vote);
  propose_vote = pbft_mgr->generateVote(alternate_propose_block_hash, propose_vote_type, 2, 1, weighted_index);
  vote_mgr->addVerifiedVote(propose_vote);

  pbft_mgr->setLastSoftVotedValue(stale_block_hash);

  uint64_t time_till_stale_ms = 1000;
  cout << "Set max wait for soft voted value to " << time_till_stale_ms << "ms...";
  pbft_mgr->setMaxWaitForSoftVotedBlock_ms(time_till_stale_ms);
  pbft_mgr->setMaxWaitForNextVotedBlock_ms(std::numeric_limits<uint64_t>::max());

  cout << "Sleep " << time_till_stale_ms << "ms so that last soft voted value of " << stale_block_hash.abridged()
       << " becomes stale..." << endl;
  taraxa::thisThreadSleepForMilliSeconds(time_till_stale_ms);

  pbft_mgr->setPbftRound(2);
  pbft_mgr->setPbftStep(2);
  pbft_mgr->resumeSingleState();

  cout << "Wait to get to cert voted state in round 2..." << endl;
  EXPECT_HAPPENS({2s, 50ms}, [&](auto &ctx) {
    auto reached_step_three_in_round_two = (pbft_mgr->getPbftRound() == 2 && pbft_mgr->getPbftStep() == 3);
    WAIT_EXPECT_EQ(ctx, reached_step_three_in_round_two, true)
  });

  cout << "Check did not soft vote for stale soft voted value of " << stale_block_hash.abridged() << "..." << endl;
  bool skipped_soft_voting = true;
  auto votes = vote_mgr->getVerifiedVotes();
  for (auto const &v : votes) {
    if (soft_vote_type == v->getType()) {
      if (v->getBlockHash() == stale_block_hash) {
        skipped_soft_voting = false;
      }
      cout << "Found soft voted value of " << v->getBlockHash().abridged() << " in round 2" << endl;
    }
  }
  EXPECT_EQ(skipped_soft_voting, true);

  auto start_round = pbft_mgr->getPbftRound();
  pbft_mgr->resume();

  cout << "Wait ensure node is still advancing in rounds... " << endl;
  EXPECT_HAPPENS({60s, 50ms}, [&](auto &ctx) { WAIT_EXPECT_NE(ctx, start_round, pbft_mgr->getPbftRound()) });
}

// Test that after some amount of elapsed time will give up next voting for some value if corresponding DAG blocks can't
// be found
TEST_F(PbftManagerTest, terminate_bogus_dag_anchor) {
  auto node_cfgs = make_node_cfgs<20>(1);
  auto nodes = launch_nodes(node_cfgs);

  auto pbft_mgr = nodes[0]->getPbftManager();
  auto db = nodes[0]->getDB();
  pbft_mgr->stop();
  cout << "Initialize PBFT manager at round 1 step 4" << endl;
  pbft_mgr->setPbftRound(1);
  pbft_mgr->setPbftStep(4);

  auto pbft_chain = nodes[0]->getPbftChain();
  auto vote_mgr = nodes[0]->getVoteManager();

  // Generate bogus DAG anchor for PBFT block
  auto dag_anchor = blk_hash_t("1234567890000000000000000000000000000000000000000000000000000000");
  auto last_pbft_block_hash = pbft_chain->getLastPbftBlockHash();
  auto propose_pbft_period = pbft_chain->getPbftChainSize() + 1;
  auto beneficiary = nodes[0]->getAddress();
  auto node_sk = nodes[0]->getSecretKey();
  auto propose_pbft_block = std::make_shared<PbftBlock>(last_pbft_block_hash, dag_anchor, blk_hash_t(),
                                                        propose_pbft_period, beneficiary, node_sk);
  auto pbft_block_hash = propose_pbft_block->getBlockHash();
  pbft_chain->pushUnverifiedPbftBlock(propose_pbft_block);

  // Generate bogus vote
  auto round = 1;
  auto step = 4;
  auto weighted_index = 0;
  auto propose_vote = pbft_mgr->generateVote(pbft_block_hash, next_vote_type, round, step, weighted_index);
  vote_mgr->addVerifiedVote(propose_vote);

  pbft_mgr->start();

  // Vote at the bogus PBFT block hash
  EXPECT_HAPPENS({10s, 50ms}, [&](auto &ctx) {
    auto soft_vote_value = blk_hash_t(0);
    auto votes = vote_mgr->getVerifiedVotes();
    for (auto const &v : votes) {
      if (soft_vote_type == v->getType() && v->getBlockHash() == pbft_block_hash) {
        soft_vote_value = v->getBlockHash();
        break;
      }
    }
    WAIT_EXPECT_EQ(ctx, soft_vote_value, pbft_block_hash)
  });

  auto start_round = pbft_mgr->getPbftRound();

  cout << "After some time, terminate voting on the bogus value " << pbft_block_hash << endl;
  EXPECT_HAPPENS({10s, 50ms}, [&](auto &ctx) {
    auto next_vote_value = pbft_block_hash;
    auto votes = vote_mgr->getVerifiedVotes();

    for (auto const &v : votes) {
      if (propose_vote_type == v->getType() && v->getBlockHash() == NULL_BLOCK_HASH) {
        auto soft_voted_from_db = *db->getPbftMgrVotedValue(PbftMgrVotedValue::SoftVotedBlockHashInRound);
        if (soft_voted_from_db == NULL_BLOCK_HASH) {
          next_vote_value = v->getBlockHash();
          break;
        }
      }
    }
    WAIT_EXPECT_EQ(ctx, next_vote_value, NULL_BLOCK_HASH)
  });

  cout << "Wait ensure node is still advancing in rounds... " << endl;
  EXPECT_HAPPENS({60s, 50ms}, [&](auto &ctx) { WAIT_EXPECT_NE(ctx, start_round, pbft_mgr->getPbftRound()) });
}

// Test that after some number of rounds will give up proposing a value if proposed block is not available
TEST_F(PbftManagerTest, terminate_missing_proposed_pbft_block) {
  auto node_cfgs = make_node_cfgs<20>(1);
  auto nodes = launch_nodes(node_cfgs);

  auto pbft_mgr = nodes[0]->getPbftManager();
  auto db = nodes[0]->getDB();
  pbft_mgr->stop();
  cout << "Initialize PBFT manager at round 1 step 4" << endl;
  pbft_mgr->setPbftRound(1);
  pbft_mgr->setPbftStep(4);

  auto pbft_chain = nodes[0]->getPbftChain();
  auto vote_mgr = nodes[0]->getVoteManager();

  auto node_sk = nodes[0]->getSecretKey();

  // Generate bogus vote
  auto round = 1;
  auto step = 4;
  auto weighted_index = 0;
  auto pbft_block_hash = blk_hash_t("0000000100000000000000000000000000000000000000000000000000000000");
  auto propose_vote = pbft_mgr->generateVote(pbft_block_hash, next_vote_type, round, step, weighted_index);
  vote_mgr->addVerifiedVote(propose_vote);

  pbft_mgr->start();

  // Vote at the bogus PBFT block hash
  EXPECT_HAPPENS({10s, 50ms}, [&](auto &ctx) {
    auto soft_vote_value = NULL_BLOCK_HASH;
    auto votes = vote_mgr->getVerifiedVotes();
    for (auto const &v : votes) {
      if (soft_vote_type == v->getType() && v->getBlockHash() == pbft_block_hash) {
        soft_vote_value = v->getBlockHash();
        break;
      }
    }
    WAIT_EXPECT_EQ(ctx, soft_vote_value, pbft_block_hash)
  });

  auto start_round = pbft_mgr->getPbftRound();

  cout << "After some time, terminate voting on the missing proposed block " << pbft_block_hash << endl;
  // After some rounds, terminate the bogus value and vote on NULL_BLOCK_HASH since no new DAG blocks generated
  EXPECT_HAPPENS({10s, 50ms}, [&](auto &ctx) {
    auto soft_vote_value = pbft_block_hash;
    auto votes = vote_mgr->getVerifiedVotes();

    for (auto const &v : votes) {
      if (propose_vote_type == v->getType() && v->getBlockHash() == NULL_BLOCK_HASH) {
        auto soft_voted_from_db = *db->getPbftMgrVotedValue(PbftMgrVotedValue::SoftVotedBlockHashInRound);
        if (soft_voted_from_db == NULL_BLOCK_HASH) {
          soft_vote_value = v->getBlockHash();
          break;
        }
      }
    }
    WAIT_EXPECT_EQ(ctx, soft_vote_value, NULL_BLOCK_HASH)
  });

  cout << "Wait ensure node is still advancing in rounds... " << endl;
  EXPECT_HAPPENS({60s, 50ms}, [&](auto &ctx) { WAIT_EXPECT_NE(ctx, start_round, pbft_mgr->getPbftRound()) });
}

TEST_F(PbftManagerTest, full_node_lambda_input_test) {
  auto node = create_nodes(1, true /*start*/).front();

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
    nodes[0]->getTransactionManager()->insertTransaction(trx, false);

    trxs_count++;
    EXPECT_HAPPENS({120s, 1s}, [&](auto &ctx) {
      for (auto &node : nodes) {
        if (ctx.fail_if(!node->getFinalChain()->transaction_location(trx.getHash()))) {
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
    nodes[0]->getTransactionManager()->insertTransaction(master_boot_node_send_coins, false);
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
        nodes[0]->getTransactionManager()->insertTransaction(dummy_trx, false);
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
    nodes[i]->getTransactionManager()->insertTransaction(send_coins_in_robin_cycle, false);
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
  auto node = create_nodes(node_cfgs, true /*start*/).front();

  // create a transaction
  auto coins_value = val_t(100);
  auto gas_price = val_t(2);
  auto receiver = addr_t("973ecb1c08c8eb5a7eaa0d3fd3aab7924f2838b0");
  auto data = bytes();
  Transaction trx_master_boot_node_to_receiver(0, coins_value, gas_price, TEST_TX_GAS_LIMIT, data, node->getSecretKey(),
                                               receiver);
  node->getTransactionManager()->insertTransaction(trx_master_boot_node_to_receiver, false);

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
  nodes[0]->getTransactionManager()->insertTransaction(trx_master_boot_node_to_node2, false);

  std::cout << "Checking all nodes see transaction from node 1 to node 2..." << std::endl;

  const expected_balances_map_t expected_balances1 = {
      {node1_addr, node1_genesis_bal - 100}, {node2_addr, 100}, {node3_addr, 0}};
  wait_for_balances(nodes, expected_balances1);

  // create a transaction transfer coins from node1 to node3
  auto coins_value3 = val_t(1000);
  Transaction trx_master_boot_node_to_node3(1, coins_value3, gas_price, TEST_TX_GAS_LIMIT, data,
                                            nodes[0]->getSecretKey(), node3_addr);
  // broadcast trx and insert
  nodes[0]->getTransactionManager()->insertTransaction(trx_master_boot_node_to_node3, false);

  std::cout << "Checking all nodes see transaction from node 1 to node 3..." << std::endl;

  const expected_balances_map_t expected_balances2 = {
      {node1_addr, node1_genesis_bal - 1100}, {node2_addr, 100}, {node3_addr, 1000}};
  wait_for_balances(nodes, expected_balances2);

  for (auto &node : nodes) {
    node->getPbftManager()->stop();
  }
  // PBFT second block
  blk_hash_t pbft_second_block_hash = nodes[0]->getPbftChain()->getLastPbftBlockHash();
  PbftBlock pbft_second_block = nodes[0]->getPbftChain()->getPbftBlockInChain(pbft_second_block_hash);
  for (auto &node : nodes) {
    auto db = node->getDB();
    auto dag_blocks_hash_in_schedule = db->getFinalizedDagBlockHashesByPeriod(pbft_second_block.getPeriod());
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
    dag_blocks_hash_in_schedule = db->getFinalizedDagBlockHashesByPeriod(pbft_first_block.getPeriod());
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
