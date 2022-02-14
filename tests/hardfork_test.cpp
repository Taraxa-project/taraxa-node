#include <gtest/gtest.h>

#include <atomic>
#include <iostream>
#include <mutex>
#include <shared_mutex>
#include <vector>

#include "cli/config.hpp"
#include "cli/tools.hpp"
#include "dag/dag.hpp"
#include "logger/logger.hpp"
#include "node/node.hpp"
#include "string"
#include "transaction_manager/transaction_manager.hpp"
#include "util_test/samples.hpp"

namespace taraxa::core_tests {

// We need separate fixture for this tests because hardfork is overwriting config file. But we can't change config
// stored in global variable because values will change for next test cases
struct HardforkTest : WithDataDir {
  FullNodeConfig node_cfg;

  HardforkTest() {
    auto cfg_filename = std::string("conf_taraxa1.json");
    auto p = DIR_CONF / cfg_filename;
    auto w = DIR_CONF / std::string("wallet1.json");
    Json::Value test_node_wallet_json;
    std::ifstream(w.string(), std::ifstream::binary) >> test_node_wallet_json;
    node_cfg = FullNodeConfig(p.string(), test_node_wallet_json, data_dir / cfg_filename);

    fs::remove_all(node_cfg.data_path);
    fs::create_directories(node_cfg.data_path);

    auto data_path_cfg = node_cfg.data_path / fs::path(node_cfg.json_file_name).filename();
    fs::copy_file(node_cfg.json_file_name, data_path_cfg);
    node_cfg.json_file_name = data_path_cfg;
    // speed up block production
    {
      node_cfg.chain.sortition.vrf.threshold_upper = 0xffff;
      node_cfg.chain.sortition.vrf.threshold_range = 0xa;
      node_cfg.chain.sortition.vdf.difficulty_min = 0;
      node_cfg.chain.sortition.vdf.difficulty_max = 3;
      node_cfg.chain.sortition.vdf.difficulty_stale = 3;
      node_cfg.chain.sortition.vdf.lambda_bound = 100;
      // PBFT config
      node_cfg.chain.pbft.lambda_ms_min /= 20;
      node_cfg.network.network_transaction_interval /= 20;
    }
  }

  ~HardforkTest() { fs::remove_all(node_cfg.data_path); }
};

TEST_F(HardforkTest, hardfork_override) {
  auto default_json = cli::Tools::generateConfig(cli::Config::DEFAULT_NETWORK_ID);
  auto default_hardforks = default_json["chain_config"]["final_chain"]["state"]["hardforks"];
  Json::Value config = default_json;
  auto &state_cfg = config["chain_config"]["final_chain"]["state"];
  state_cfg["hardforks"].removeMember("fix_genesis_fork_block");

  EXPECT_TRUE(state_cfg["hardforks"]["fix_genesis_fork_block"].isNull());
  cli::Config::addNewHardforks(config, default_json);
  EXPECT_EQ(state_cfg["hardforks"], default_hardforks);

  state_cfg.removeMember("hardforks");
  EXPECT_TRUE(state_cfg["hardforks"].isNull());

  cli::Config::addNewHardforks(config, default_json);
  EXPECT_EQ(state_cfg["hardforks"], default_hardforks);
}

TEST_F(HardforkTest, fix_genesis_fork_block_is_zero) {
  auto &cfg = node_cfg.chain.final_chain;
  cfg.state.hardforks.fix_genesis_fork_block = 0;
  auto node = launch_nodes({node_cfg}).front();

  Transaction dummy_trx(1, 0, 0, 0, bytes(), node->getSecretKey(), node->getAddress());
  // broadcast dummy transaction
  node->getTransactionManager()->insertTransaction(dummy_trx);
  wait({100s, 500ms}, [&](auto &ctx) {
    if (node->getFinalChain()->last_block_number() <= cfg.state.hardforks.fix_genesis_fork_block) {
      ctx.fail();
    }
  });
  EXPECT_EQ(cfg.state.genesis_balances.begin()->second,
            node->getConfig().chain.final_chain.state.genesis_balances.begin()->second);
}

TEST_F(HardforkTest, hardfork) {
  auto &cfg = node_cfg.chain.final_chain;
  cfg.state.hardforks.fix_genesis_fork_block = 10;
  cfg.state.dpos->eligibility_balance_threshold = 100000;
  cfg.state.dpos->vote_eligibility_balance_step.assign(cfg.state.dpos->eligibility_balance_threshold);
  cfg.state.dpos->deposit_delay = 5;
  cfg.state.dpos->withdrawal_delay = 5;

  auto random_node = addr_t::random();
  auto random_votes = 3;
  for (auto &gb : cfg.state.genesis_balances) {
    gb.second = 110000000;
  }
  for (auto &gs : cfg.state.dpos->genesis_state) {
    for (auto &b : gs.second) {
      b.second = 1100000;
      std::cout << b.first << ": " << b.second << std::endl;
    }
    gs.second.emplace(random_node, random_votes * cfg.state.dpos->vote_eligibility_balance_step);
  }

  auto node = launch_nodes({node_cfg}).front();
  auto nonce = 0;
  auto dummy_trx = [&nonce, node]() {
    Transaction dummy_trx(nonce++, 0, 0, 0, bytes(), node->getSecretKey(), node->getAddress());
    // broadcast dummy transaction
    node->getTransactionManager()->insertTransaction(dummy_trx);
  };
  dummy_trx();
  node->getFinalChain()->block_finalized_.subscribe([&](const std::shared_ptr<final_chain::FinalizationResult> &res) {
    const auto block_num = res->final_chain_blk->number;
    if (cfg.state.hardforks.fix_genesis_fork_block == block_num) {
      return;
    }
    dummy_trx();
    dummy_trx();
  });
  std::map<addr_t, u256> balances_before;
  for (const auto &b : node->getConfig().chain.final_chain.state.genesis_balances) {
    auto balance = node->getFinalChain()->get_account(b.first)->balance;
    balances_before.emplace(b.first, balance);
  }
  auto votes_count = 11;
  EXPECT_EQ(2, node->getFinalChain()->dpos_eligible_count(0));
  EXPECT_EQ(votes_count + random_votes, node->getFinalChain()->dpos_eligible_total_vote_count(0));
  EXPECT_EQ(random_votes, node->getFinalChain()->dpos_eligible_vote_count(0, random_node));

  wait({100s, 500ms}, [&](auto &ctx) {
    if (node->getFinalChain()->last_block_number() < cfg.state.hardforks.fix_genesis_fork_block) {
      ctx.fail();
    }
  });

  u256 dpos_genesis_sum = 0;
  // Verify DPOS initial balances increasing
  for (const auto &gs : node->getConfig().chain.final_chain.state.dpos->genesis_state) {
    for (const auto &b : gs.second) {
      EXPECT_EQ(b.second, node->getFinalChain()->get_staking_balance(b.first));
      dpos_genesis_sum += b.second;
    }
  }

  for (const auto &b : node->getConfig().chain.final_chain.state.genesis_balances) {
    auto balance_after = node->getFinalChain()->get_account(b.first)->balance;
    auto res = b.second - dpos_genesis_sum;
    EXPECT_EQ(res, balance_after);
  }

  auto block = node->getFinalChain()->last_block_number();
  EXPECT_EQ(1, node->getFinalChain()->dpos_eligible_count(block));
  EXPECT_EQ(votes_count, node->getFinalChain()->dpos_eligible_total_vote_count(block));
  EXPECT_EQ(0, node->getFinalChain()->dpos_eligible_vote_count(block, random_node));

  // check for dpos_query method
  {
    const auto &genesis_sender = cfg.state.dpos->genesis_state.begin()->first;

    state_api::DPOSQuery::AccountQuery acc_q;
    acc_q.with_staking_balance = true;
    acc_q.with_outbound_deposits = true;
    acc_q.with_inbound_deposits = true;
    state_api::DPOSQuery q;
    q.with_eligible_count = true;
    q.account_queries[genesis_sender] = acc_q;

    auto q_res = node->getFinalChain()->dpos_query(q);
    auto res = q_res.account_results[genesis_sender];
    EXPECT_EQ(res.inbound_deposits.size(), 1);
    EXPECT_EQ(res.inbound_deposits.begin()->first, genesis_sender);
    EXPECT_EQ(res.inbound_deposits.begin()->second, res.staking_balance);
  }

  EXPECT_EQ(cfg.state.dpos->vote_eligibility_balance_step * kOneTara,
            node->getConfig().chain.final_chain.state.dpos->vote_eligibility_balance_step);
  EXPECT_NE(cfg.state.genesis_balances.begin()->second,
            node->getConfig().chain.final_chain.state.genesis_balances.begin()->second);
  EXPECT_NE(cfg.state.dpos->eligibility_balance_threshold,
            node->getConfig().chain.final_chain.state.dpos->eligibility_balance_threshold);
}

}  // namespace taraxa::core_tests