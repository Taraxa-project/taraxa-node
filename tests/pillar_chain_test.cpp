#include <gtest/gtest.h>

#include <iostream>

#include "common/static_init.hpp"
#include "logger/logger.hpp"
#include "pbft/pbft_manager.hpp"
#include "pillar_chain/pillar_chain_manager.hpp"
#include "test_util/test_util.hpp"

namespace taraxa::core_tests {

struct PillarChainTest : NodesTest {};

TEST_F(PillarChainTest, pillar_chain_db) {
  auto db_ptr = std::make_shared<DbStorage>(data_dir);
  auto& db = *db_ptr;

  // Pillar chain
  EthBlockNumber pillar_block_num(123);
  h256 state_root(456);
  blk_hash_t previous_pillar_block_hash(789);

  std::vector<pillar_chain::PillarBlock::ValidatorStakeChange> stakes_changes;
  const auto stake_change1 = stakes_changes.emplace_back(addr_t(1), dev::s256(1));
  const auto stake_change2 = stakes_changes.emplace_back(addr_t(2), dev::s256(2));

  const auto pillar_block = std::make_shared<pillar_chain::PillarBlock>(
      pillar_block_num, state_root, std::move(stakes_changes), previous_pillar_block_hash);

  // Pillar block
  auto batch = db.createWriteBatch();
  db.saveCurrentPillarBlock(pillar_block, batch);
  db.commitWriteBatch(batch);

  const auto pillar_block_db = db.getCurrentPillarBlock();
  EXPECT_EQ(pillar_block->getHash(), pillar_block_db->getHash());

  // Pillar block data
  std::vector<std::shared_ptr<PillarVote>> pillar_votes;
  const auto vote1 = pillar_votes.emplace_back(
      std::make_shared<PillarVote>(secret_t::random(), pillar_block->getPeriod(), pillar_block->getHash()));
  const auto vote2 = pillar_votes.emplace_back(
      std::make_shared<PillarVote>(secret_t::random(), pillar_block->getPeriod(), pillar_block->getHash()));

  const auto previous_pillar_block = std::make_shared<pillar_chain::PillarBlock>(
      pillar_block_num - 1, h256{}, std::vector<pillar_chain::PillarBlock::ValidatorStakeChange>{}, blk_hash_t{});
  db.savePillarBlockData(
      pillar_chain::PillarBlockData{pillar_block, std::vector<std::shared_ptr<PillarVote>>{pillar_votes}});
  db.savePillarBlockData(
      pillar_chain::PillarBlockData{previous_pillar_block, std::vector<std::shared_ptr<PillarVote>>{pillar_votes}});

  const auto pillar_block_data_db = db.getPillarBlockData(pillar_block->getPeriod());
  EXPECT_EQ(pillar_block->getHash(), pillar_block_data_db->block_->getHash());
  EXPECT_EQ(pillar_votes.size(), pillar_block_data_db->pillar_votes_.size());
  for (size_t idx = 0; idx < pillar_votes.size(); idx++) {
    EXPECT_EQ(pillar_votes[idx]->getHash(), pillar_block_data_db->pillar_votes_[idx]->getHash());
  }

  const auto latest_pillar_block_data_db = db.getLatestPillarBlockData();
  EXPECT_EQ(pillar_block->getHash(), latest_pillar_block_data_db->block_->getHash());
  EXPECT_EQ(pillar_votes.size(), latest_pillar_block_data_db->pillar_votes_.size());
  for (size_t idx = 0; idx < pillar_votes.size(); idx++) {
    EXPECT_EQ(pillar_votes[idx]->getHash(), latest_pillar_block_data_db->pillar_votes_[idx]->getHash());
  }

  // Pillar block stakes
  std::vector<state_api::ValidatorStake> stakes;
  const auto stake1 = stakes_changes.emplace_back(addr_t(123), dev::s256(123));
  const auto stake2 = stakes_changes.emplace_back(addr_t(456), dev::s256(456));

  batch = db.createWriteBatch();
  db.saveCurrentPillarBlockStakes(stakes, batch);
  db.commitWriteBatch(batch);

  const auto current_pillar_block_stakes_db = db.getCurrentPillarBlockStakes();
  EXPECT_EQ(stakes.size(), current_pillar_block_stakes_db.size());
  for (size_t idx = 0; idx < current_pillar_block_stakes_db.size(); idx++) {
    EXPECT_EQ(current_pillar_block_stakes_db[idx].addr, current_pillar_block_stakes_db[idx].addr);
    EXPECT_EQ(current_pillar_block_stakes_db[idx].stake, current_pillar_block_stakes_db[idx].stake);
  }

  // Pillar vote
  auto pillar_vote =
      std::make_shared<PillarVote>(secret_t::random(), pillar_block->getPeriod(), pillar_block->getHash());
  db.saveOwnPillarBlockVote(pillar_vote);
  auto pillar_vote_db = db.getOwnPillarBlockVote();
  EXPECT_EQ(pillar_vote->getHash(), pillar_vote_db->getHash());
}

TEST_F(PillarChainTest, pillar_blocks_create) {
  auto node_cfgs = make_node_cfgs(2, 2, 10);
  for (auto& node_cfg : node_cfgs) {
    node_cfg.genesis.state.dpos.delegation_delay = 1;
    node_cfg.genesis.state.hardforks.ficus_hf.block_num = 0;
    node_cfg.genesis.state.hardforks.ficus_hf.pillar_block_periods = 4;
    node_cfg.genesis.state.hardforks.ficus_hf.pillar_chain_sync_periods = 3;
    node_cfg.genesis.state.hardforks.ficus_hf.pbft_inclusion_delay = 2;
  }

  auto nodes = launch_nodes(node_cfgs);

  // Wait until nodes create at least 2 pillar blocks
  const auto pillar_blocks_count = 2;
  const auto min_amount_of_pbft_blocks =
      pillar_blocks_count * node_cfgs[0].genesis.state.hardforks.ficus_hf.pillar_block_periods;
  ASSERT_HAPPENS({20s, 250ms}, [&](auto& ctx) {
    for (const auto& node : nodes) {
      WAIT_EXPECT_GE(ctx, node->getPbftChain()->getPbftChainSize(),
                     min_amount_of_pbft_blocks + node_cfgs[0].genesis.state.hardforks.ficus_hf.pbft_inclusion_delay)
    }
  });

  for (auto& node : nodes) {
    node->getPbftManager()->stop();

    // Check if right amount of pillar blocks were created
    const auto latest_pillar_block_data = node->getDB()->getLatestPillarBlockData();
    ASSERT_TRUE(latest_pillar_block_data.has_value());
    ASSERT_EQ(latest_pillar_block_data->block_->getPeriod(),
              pillar_blocks_count * node_cfgs[0].genesis.state.hardforks.ficus_hf.pillar_block_periods);
  }
}

TEST_F(PillarChainTest, stakes_changes) {
  const auto validators_count = 3;
  auto node_cfgs = make_node_cfgs(validators_count, validators_count, 10);

  for (auto& node_cfg : node_cfgs) {
    node_cfg.genesis.state.dpos.delegation_delay = 1;
    node_cfg.genesis.state.hardforks.ficus_hf.block_num = 0;
    node_cfg.genesis.state.hardforks.ficus_hf.pillar_block_periods = 4;
    node_cfg.genesis.state.hardforks.ficus_hf.pillar_chain_sync_periods = 3;
    node_cfg.genesis.state.hardforks.ficus_hf.pbft_inclusion_delay = 2;
  }

  std::vector<dev::s256> validators_stakes;
  validators_stakes.reserve(node_cfgs.size());
  for (const auto& validator : node_cfgs[0].genesis.state.dpos.initial_validators) {
    auto& stake = validators_stakes.emplace_back(0);
    for (const auto& delegation : validator.delegations) {
      stake += delegation.second;
    }
  }

  auto nodes = launch_nodes(node_cfgs);

  // Wait until nodes create first pillar block
  const auto first_pillar_block_period = node_cfgs[0].genesis.state.hardforks.ficus_hf.firstPillarBlockPeriod();
  ASSERT_HAPPENS({20s, 250ms}, [&](auto& ctx) {
    for (const auto& node : nodes) {
      WAIT_EXPECT_GE(ctx, node->getPbftChain()->getPbftChainSize(),
                     first_pillar_block_period + node_cfgs[0].genesis.state.hardforks.ficus_hf.pbft_inclusion_delay)
    }
  });

  // Check if stakes changes in first pillar block == initial validators stakes
  for (auto& node : nodes) {
    // Check if right amount of pillar blocks were created
    const auto first_pillar_block_data = node->getDB()->getPillarBlockData(first_pillar_block_period);
    ASSERT_TRUE(first_pillar_block_data.has_value());

    ASSERT_EQ(first_pillar_block_data->block_->getPeriod(), first_pillar_block_period);
    ASSERT_EQ(first_pillar_block_data->block_->getValidatorsStakesChanges().size(), validators_count);
    size_t idx = 0;
    for (const auto& stake_change : first_pillar_block_data->block_->getValidatorsStakesChanges()) {
      ASSERT_EQ(stake_change.stake_change_, validators_stakes[idx]);
      idx++;
    }
  }

  // Change validators delegation
  const auto delegation_value = 2 * node_cfgs[0].genesis.state.dpos.eligibility_balance_threshold;
  const auto stake_change_validators_count = validators_count - 1;
  for (size_t i = 0; i < stake_change_validators_count; i++) {
    const auto trx = make_delegate_tx(node_cfgs[i], delegation_value, 1, 1000);
    nodes[0]->getTransactionManager()->insertTransaction(trx);
  }

  PbftPeriod executed_delegations_pbft_period = 1000000;
  EXPECT_HAPPENS({20s, 1s}, [&](auto& ctx) {
    for (auto& node : nodes) {
      if (ctx.fail_if(node->getDB()->getNumTransactionExecuted() != stake_change_validators_count)) {
        return;
      }
      const auto chain_size = node->getPbftChain()->getPbftChainSize();
      if (chain_size < executed_delegations_pbft_period) {
        executed_delegations_pbft_period = chain_size;
      }
    }
  });

  // Wait until new pillar block with changed validators stakes is created
  const auto new_pillar_block_period =
      executed_delegations_pbft_period -
      executed_delegations_pbft_period % node_cfgs[0].genesis.state.hardforks.ficus_hf.pillar_block_periods +
      node_cfgs[0].genesis.state.hardforks.ficus_hf.pillar_block_periods;
  ASSERT_HAPPENS({20s, 250ms}, [&](auto& ctx) {
    for (const auto& node : nodes) {
      WAIT_EXPECT_GE(ctx, node->getPbftChain()->getPbftChainSize(),
                     new_pillar_block_period + node_cfgs[0].genesis.state.hardforks.ficus_hf.pbft_inclusion_delay)
    }
  });

  // Check if stakes changes in new pillar block changed according to new delegations
  for (auto& node : nodes) {
    // Check if right amount of pillar blocks were created
    const auto new_pillar_block_data = node->getDB()->getPillarBlockData(new_pillar_block_period);
    ASSERT_TRUE(new_pillar_block_data.has_value());
    ASSERT_EQ(new_pillar_block_data->block_->getPeriod(), new_pillar_block_period);
    ASSERT_EQ(new_pillar_block_data->block_->getValidatorsStakesChanges().size(), stake_change_validators_count);
    size_t idx = 0;
    for (const auto& stake_change : new_pillar_block_data->block_->getValidatorsStakesChanges()) {
      ASSERT_EQ(stake_change.stake_change_, delegation_value);
      idx++;
    }
  }
}

TEST_F(PillarChainTest, pillar_chain_syncing) {
  auto node_cfgs = make_node_cfgs(2, 1, 10);

  for (auto& node_cfg : node_cfgs) {
    node_cfg.genesis.state.dpos.delegation_delay = 1;
    node_cfg.genesis.state.hardforks.ficus_hf.block_num = 0;
    node_cfg.genesis.state.hardforks.ficus_hf.pillar_block_periods = 4;
    node_cfg.genesis.state.hardforks.ficus_hf.pillar_chain_sync_periods = 3;
    node_cfg.genesis.state.hardforks.ficus_hf.pbft_inclusion_delay = 2;
  }

  // Start first node
  auto node1 = launch_nodes({node_cfgs[0]})[0];

  // Wait until node1 creates at least 3 pillar blocks
  const auto pillar_blocks_count = 3;
  ASSERT_HAPPENS({20s, 250ms}, [&](auto& ctx) {
    WAIT_EXPECT_GE(ctx, node1->getPbftChain()->getPbftChainSize(),
                   pillar_blocks_count * node_cfgs[0].genesis.state.hardforks.ficus_hf.pillar_block_periods +
                       node_cfgs[0].genesis.state.hardforks.ficus_hf.pbft_inclusion_delay)
  });
  node1->getPbftManager()->stop();

  // Start second node
  auto node2 = launch_nodes({node_cfgs[1]})[0];
  // Wait until node2 syncs pbft chain with node1
  ASSERT_HAPPENS({20s, 250ms}, [&](auto& ctx) {
    WAIT_EXPECT_EQ(ctx, node2->getPbftChain()->getPbftChainSize(), node1->getPbftChain()->getPbftChainSize())
  });

  // Pbft/pillar chain syncing works in a way that pbft block with period N contains pillar votes for pillar block with
  // period N-ficus_hf.pillar_block_periods.
  const auto node2_latest_finalized_pillar_block_data = node2->getDB()->getLatestPillarBlockData();
  ASSERT_TRUE(node2_latest_finalized_pillar_block_data.has_value());
  ASSERT_EQ(node2_latest_finalized_pillar_block_data->block_->getPeriod(),
            (pillar_blocks_count - 1) * node_cfgs[0].genesis.state.hardforks.ficus_hf.pillar_block_periods);

  // Trigger pillar votes syncing
  node2->getPillarChainManager()->checkPillarChainSynced(
      pillar_blocks_count * node_cfgs[0].genesis.state.hardforks.ficus_hf.pillar_block_periods);
  // Wait until node2 gets pillar votes and finalized pillar block #3
  ASSERT_HAPPENS({20s, 250ms}, [&](auto& ctx) {
    WAIT_EXPECT_EQ(ctx, node2->getDB()->getLatestPillarBlockData()->block_->getPeriod(),
                   pillar_blocks_count * node_cfgs[0].genesis.state.hardforks.ficus_hf.pillar_block_periods)
  });
}
}  // namespace taraxa::core_tests

using namespace taraxa;
int main(int argc, char** argv) {
  taraxa::static_init();
  auto logging = logger::createDefaultLoggingConfig();
  logging.verbosity = logger::Verbosity::Debug;

  addr_t node_addr;
  logger::InitLogging(logging, node_addr);

  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}