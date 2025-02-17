#include "rewards/rewards_stats.hpp"

#include <gtest/gtest.h>
#include <libdevcore/Address.h>
#include <libdevcore/Common.h>
#include <libdevcore/CommonJS.h>

#include "storage/storage.hpp"
#include "test_util/gtest.hpp"
#include "test_util/samples.hpp"

namespace taraxa::core_tests {

auto g_secret = dev::Secret("3800b2875669d9b2053c1aff9224ecfdc411423aac5b5a73d7a45ced1c3b9dcd",
                            dev::Secret::ConstructFromStringType::FromHex);

struct RewardsStatsTest : NodesTest {};

class TestableRewardsStats : public rewards::Stats {
 public:
  TestableRewardsStats(const HardforksConfig::RewardsDistributionMap& rdm, std::shared_ptr<DbStorage> db)
      : rewards::Stats(100,
                       HardforksConfig{0,
                                       {},
                                       rdm,
                                       MagnoliaHardfork{0, 0},
                                       0,
                                       0,
                                       AspenHardfork{0, 0},
                                       FicusHardforkConfig{0, 0, {}},
                                       CornusHardforkConfig{0, 0, 0, 0},
                                       SoleiroliaHardforkConfig{0, 0, 0},
                                       CactiHardforkConfig{}},
                       db, [](auto) { return 100; }) {}
  auto getStats() { return blocks_stats_; }
};

class TestableBlockStats : public rewards::BlockStats {
 public:
  const addr_t& getAuthor() const { return block_author_; }
  const auto& getValidatorStats() const { return validators_stats_; }
};

TEST_F(RewardsStatsTest, defaultDistribution) {
  auto db = std::make_shared<DbStorage>(data_dir / "db");
  auto batch = db->createWriteBatch();

  std::vector<std::shared_ptr<PbftVote>> empty_votes;
  auto rewards_stats = TestableRewardsStats({}, db);

  for (auto i = 1; i < 5; ++i) {
    PeriodData block(make_simple_pbft_block(blk_hash_t(i), i), empty_votes);
    auto stats = rewards_stats.processStats(block, {}, batch);
    ASSERT_EQ(stats.size(), 1);
    ASSERT_TRUE(rewards_stats.getStats().empty());
  }
  db->commitWriteBatch(batch);
}

TEST_F(RewardsStatsTest, statsSaving) {
  auto db = std::make_shared<DbStorage>(data_dir / "db");

  // distribute every 5 blocks
  HardforksConfig::RewardsDistributionMap distribution{{0, 5}};

  std::vector<std::shared_ptr<PbftVote>> empty_votes;
  std::vector<addr_t> block_authors;
  {
    auto rewards_stats = TestableRewardsStats(distribution, db);
    auto batch = db->createWriteBatch();

    for (auto i = 1; i < 5; ++i) {
      auto kp = dev::KeyPair::create();
      block_authors.push_back(kp.address());

      PeriodData block(make_simple_pbft_block(blk_hash_t(i), i, kp.secret()), empty_votes);
      auto stats = rewards_stats.processStats(block, {}, batch);
      ASSERT_EQ(rewards_stats.getStats().size(), block_authors.size());
      ASSERT_TRUE(stats.empty());
    }
    db->commitWriteBatch(batch);
  }
  {
    // Load from db
    auto rewards_stats = TestableRewardsStats(distribution, db);
    auto stats = rewards_stats.getStats();
    ASSERT_EQ(rewards_stats.getStats().size(), block_authors.size());

    for (size_t i = 1; i <= stats.size(); ++i) {
      auto stats_with_get = reinterpret_cast<TestableBlockStats*>(&stats[i]);
      ASSERT_EQ(stats_with_get->getAuthor(), block_authors[i - 1]);
    }
  }
}

TEST_F(RewardsStatsTest, statsCleaning) {
  auto db = std::make_shared<DbStorage>(data_dir / "db");
  auto batch = db->createWriteBatch();

  // distribute every 5 blocks
  HardforksConfig::RewardsDistributionMap distribution{{0, 5}};

  std::vector<std::shared_ptr<PbftVote>> empty_votes;
  std::vector<addr_t> block_authors;
  {
    auto rewards_stats = TestableRewardsStats(distribution, db);

    for (auto i = 1; i < 5; ++i) {
      auto kp = dev::KeyPair::create();
      block_authors.push_back(kp.address());

      PeriodData block(make_simple_pbft_block(blk_hash_t(i), i, kp.secret()), empty_votes);
      auto stats = rewards_stats.processStats(block, {}, batch);
      ASSERT_EQ(rewards_stats.getStats().size(), block_authors.size());
      ASSERT_TRUE(stats.empty());
    }
    db->commitWriteBatch(batch);
    // Process block 5 after which we should have no stats elements in db
    PeriodData block(make_simple_pbft_block(blk_hash_t(5), 5), empty_votes);
    rewards_stats.processStats(block, {}, batch);
    db->commitWriteBatch(batch);
    rewards_stats.clear(block.pbft_blk->getPeriod());
  }

  // Load from db
  auto rewards_stats = TestableRewardsStats(distribution, db);
  ASSERT_TRUE(rewards_stats.getStats().empty());
}

TEST_F(RewardsStatsTest, statsProcessing) {
  auto db = std::make_shared<DbStorage>(data_dir / "db");
  // distribute every 10 blocks
  auto rewards_stats = TestableRewardsStats({{0, 10}}, db);
  auto batch = db->createWriteBatch();

  std::vector<std::shared_ptr<PbftVote>> empty_votes;
  std::vector<addr_t> block_authors;

  // make blocks [1,9] and process them. output of processStats should be empty
  for (auto i = 1; i < 10; ++i) {
    auto kp = dev::KeyPair::create();
    block_authors.push_back(kp.address());

    PeriodData block(make_simple_pbft_block(blk_hash_t(i), i, kp.secret()), empty_votes);
    auto stats = rewards_stats.processStats(block, {}, batch);
    ASSERT_TRUE(stats.empty());
    ASSERT_EQ(rewards_stats.getStats().size(), block_authors.size());
  }

  auto kp = dev::KeyPair::create();
  block_authors.push_back(kp.address());

  PeriodData block(make_simple_pbft_block(blk_hash_t(10), 10, kp.secret()), empty_votes);
  auto stats = rewards_stats.processStats(block, {}, batch);
  rewards_stats.clear(block.pbft_blk->getPeriod());
  ASSERT_EQ(stats.size(), block_authors.size());

  for (auto& block_author : block_authors) {
    bool found = false;
    for (size_t i = 0; i < stats.size(); ++i) {
      auto stats_with_get = reinterpret_cast<TestableBlockStats*>(&stats[i]);
      if (stats_with_get->getAuthor() == block_author) found = true;
    }
    assert(found);
  }
  ASSERT_TRUE(rewards_stats.getStats().empty());
}

TEST_F(RewardsStatsTest, distributionChange) {
  auto db = std::make_shared<DbStorage>(data_dir / "db");
  auto batch = db->createWriteBatch();

  HardforksConfig::RewardsDistributionMap distribution{{6, 5}, {11, 2}};

  auto rewards_stats = TestableRewardsStats(distribution, db);

  std::vector<std::shared_ptr<PbftVote>> empty_votes;
  uint64_t period = 1;
  for (; period <= 5; ++period) {
    PeriodData block(make_simple_pbft_block(blk_hash_t(period), period), empty_votes);
    auto stats = rewards_stats.processStats(block, {}, batch);
    ASSERT_FALSE(stats.empty());
  }
  {
    // make blocks [1,9] and process them. output of processStats should be empty
    for (; period < 10; ++period) {
      PeriodData block(make_simple_pbft_block(blk_hash_t(period), period), empty_votes);
      auto stats = rewards_stats.processStats(block, {}, batch);
      ASSERT_TRUE(stats.empty());
    }
    PeriodData block(make_simple_pbft_block(blk_hash_t(period), period), empty_votes);
    auto stats = rewards_stats.processStats(block, {}, batch);
  }

  PeriodData block(make_simple_pbft_block(blk_hash_t(period), period), empty_votes);
  auto stats = rewards_stats.processStats(block, {}, batch);
}

TEST_F(RewardsStatsTest, feeRewards) {
  auto db = std::make_shared<DbStorage>(data_dir / "db");
  auto batch = db->createWriteBatch();
  auto pbft_proposer = dev::KeyPair::create();
  auto dag_proposer = dev::KeyPair::create();

  HardforksConfig::RewardsDistributionMap distribution{};

  auto rewards_stats = TestableRewardsStats(distribution, db);

  std::vector<std::shared_ptr<PbftVote>> empty_votes;
  uint64_t period = 1;
  uint64_t nonce = 1;

  const auto trx_gas_fee = 1000000000;
  const auto gas_limit = 1000000;
  auto trx = std::make_shared<Transaction>(nonce++, 0, trx_gas_fee, gas_limit,
                                           dev::fromHex(samples::greeter_contract_code), pbft_proposer.secret());

  auto dag_blk = std::make_shared<DagBlock>(blk_hash_t{}, level_t{}, vec_blk_t{}, vec_trx_t{trx->getHash()}, 0,
                                            VdfSortition{}, dag_proposer.secret());
  db->saveDagBlock(dag_blk);
  std::vector<vote_hash_t> reward_votes_hashes;
  auto pbft_block =
      std::make_shared<PbftBlock>(kNullBlockHash, kNullBlockHash, kNullBlockHash, kNullBlockHash, period,
                                  addr_t::random(), pbft_proposer.secret(), std::move(reward_votes_hashes));

  PeriodData period_data(pbft_block, empty_votes);
  period_data.dag_blocks.push_back(dag_blk);
  period_data.transactions = {trx};

  auto stats = rewards_stats.processStats(period_data, {gas_limit}, batch).front();

  auto testable_stats = reinterpret_cast<TestableBlockStats*>(&stats);
  auto validators_stats = testable_stats->getValidatorStats();
  // stats should be only for dag block proposer
  ASSERT_EQ(validators_stats.size(), 1);
  for (const auto& vs : validators_stats) {
    if (vs.first == dag_proposer.address()) {
      ASSERT_EQ(vs.second.dag_blocks_count_, 1);
      ASSERT_EQ(vs.second.fees_rewards_, uint256_t(trx_gas_fee) * gas_limit);
    }
  }
}

TEST_F(RewardsStatsTest, dagBlockRewards) {
  auto db = std::make_shared<DbStorage>(data_dir / "db");
  auto batch = db->createWriteBatch();

  std::vector<std::shared_ptr<PbftVote>> empty_votes;
  HardforksConfig hfc;
  hfc.aspen_hf.block_num_part_two = 4;

  // Create two reward stats to test before and after aspen hardfork part 1
  rewards::Stats pre_aspen_reward_stats(100,
                                        HardforksConfig{0,
                                                        {},
                                                        {},
                                                        MagnoliaHardfork{0, 0},
                                                        0,
                                                        0,
                                                        AspenHardfork{6, 999},
                                                        FicusHardforkConfig{0, 0, {}},
                                                        CornusHardforkConfig{0, 0, 0, 0},
                                                        SoleiroliaHardforkConfig{0, 0, 0},
                                                        CactiHardforkConfig{}},
                                        db, [](auto) { return 100; });
  rewards::Stats post_aspen_reward_stats(100,
                                         HardforksConfig{0,
                                                         {},
                                                         {},
                                                         MagnoliaHardfork{0, 0},
                                                         0,
                                                         0,
                                                         AspenHardfork{4, 999},
                                                         FicusHardforkConfig{0, 0, {}},
                                                         CornusHardforkConfig{0, 0, 0, 0},
                                                         SoleiroliaHardforkConfig{0, 0, 0},
                                                         CactiHardforkConfig{}},
                                         db, [](auto) { return 100; });

  // Create pbft block with 5 dag blocks
  auto dag_key1 = dev::KeyPair::create();
  auto dag_key2 = dev::KeyPair::create();
  auto dag_key3 = dev::KeyPair::create();
  auto dag_key4 = dev::KeyPair::create();
  auto dag_key5 = dev::KeyPair::create();
  vrf_wrapper::vrf_sk_t vrfs(
      "854821a22e1841f79f0a62409197e930eb347c05ede6456b82b07ec36acbd2fce86c6f2cd1e076ddf8eaf48cee078bd68b74063c3e229b1a"
      "5e993c791bdb56d6");
  auto trxs = samples::createSignedTrxSamples(1, 3, g_secret);

  PeriodData block(make_simple_pbft_block(blk_hash_t(1), 5), empty_votes);
  SortitionParams sortition_params(0xfff, 16, 21, 23, 0x64);

  vdf_sortition::VdfSortition vdf1(sortition_params, vrfs,
                                   vrf_wrapper::VrfSortitionBase::makeVrfInput(1, blk_hash_t(1)), 1, 1);
  auto dag_blk1 = std::make_shared<DagBlock>(blk_hash_t{}, level_t{}, vec_blk_t{}, vec_trx_t{trxs[0]->getHash()}, 0,
                                             vdf1, dag_key1.secret());
  block.dag_blocks.push_back(dag_blk1);

  vdf_sortition::VdfSortition vdf2(sortition_params, vrfs,
                                   vrf_wrapper::VrfSortitionBase::makeVrfInput(1, blk_hash_t(2)), 1, 1);
  auto dag_blk2 = std::make_shared<DagBlock>(blk_hash_t{}, level_t{}, vec_blk_t{}, vec_trx_t{trxs[1]->getHash()}, 0,
                                             vdf2, dag_key2.secret());
  block.dag_blocks.push_back(dag_blk2);

  vdf_sortition::VdfSortition vdf3(sortition_params, vrfs,
                                   vrf_wrapper::VrfSortitionBase::makeVrfInput(1, blk_hash_t(3)), 1, 1);
  auto dag_blk3 = std::make_shared<DagBlock>(blk_hash_t{}, level_t{}, vec_blk_t{}, vec_trx_t{trxs[0]->getHash()}, 0,
                                             vdf3, dag_key3.secret());
  block.dag_blocks.push_back(dag_blk3);

  vdf_sortition::VdfSortition vdf4(sortition_params, vrfs,
                                   vrf_wrapper::VrfSortitionBase::makeVrfInput(1, blk_hash_t(4)), 1, 1);
  auto dag_blk4 = std::make_shared<DagBlock>(blk_hash_t{}, level_t{}, vec_blk_t{}, vec_trx_t{trxs[1]->getHash()}, 0,
                                             vdf4, dag_key4.secret());
  block.dag_blocks.push_back(dag_blk4);

  vdf_sortition::VdfSortition vdf5(sortition_params, vrfs,
                                   vrf_wrapper::VrfSortitionBase::makeVrfInput(1, blk_hash_t(5)), 1, 1);
  auto dag_blk5 = std::make_shared<DagBlock>(blk_hash_t{}, level_t{}, vec_blk_t{}, vec_trx_t{trxs[2]->getHash()}, 0,
                                             vdf5, dag_key5.secret());
  block.dag_blocks.push_back(dag_blk5);
  block.transactions = trxs;

  ASSERT_EQ(dag_blk1->getDifficulty(), 17);
  ASSERT_EQ(dag_blk2->getDifficulty(), 17);
  ASSERT_EQ(dag_blk3->getDifficulty(), 16);
  ASSERT_EQ(dag_blk4->getDifficulty(), 17);
  ASSERT_EQ(dag_blk5->getDifficulty(), 16);

  std::vector<gas_t> gas_used{10, 20, 30};

  // Process rewards before aspen hf, expect dag_blocks_count to match blocks that include unique transactions which is
  // blocks 1, 2 and 5
  auto stats = pre_aspen_reward_stats.processStats(block, gas_used, batch);
  ASSERT_EQ(stats.size(), 1);
  auto stats_with_get = reinterpret_cast<TestableBlockStats*>(&stats[0]);
  ASSERT_EQ(stats_with_get->getValidatorStats().size(), 3);
  ASSERT_TRUE(stats_with_get->getValidatorStats().contains(dev::toAddress(dag_key1.pub())));
  ASSERT_TRUE(stats_with_get->getValidatorStats().contains(dev::toAddress(dag_key2.pub())));
  ASSERT_TRUE(stats_with_get->getValidatorStats().contains(dev::toAddress(dag_key5.pub())));
  ASSERT_EQ(stats_with_get->getValidatorStats().find(dev::toAddress(dag_key1.pub()))->second.dag_blocks_count_, 1);
  ASSERT_EQ(stats_with_get->getValidatorStats().find(dev::toAddress(dag_key2.pub()))->second.dag_blocks_count_, 1);
  ASSERT_EQ(stats_with_get->getValidatorStats().find(dev::toAddress(dag_key5.pub()))->second.dag_blocks_count_, 1);

  // Process rewards after aspen hf, expect dag_blocks_count to match blocks with smallest difficulty which is blocks 3
  // and 5 Verify fees rewards to be the same before and after the HF
  auto post_stats = post_aspen_reward_stats.processStats(block, gas_used, batch);
  ASSERT_EQ(post_stats.size(), 1);
  auto post_stats_with_get = reinterpret_cast<TestableBlockStats*>(&post_stats[0]);
  ASSERT_EQ(post_stats_with_get->getValidatorStats().size(), 4);
  ASSERT_TRUE(post_stats_with_get->getValidatorStats().contains(dev::toAddress(dag_key1.pub())));
  ASSERT_TRUE(post_stats_with_get->getValidatorStats().contains(dev::toAddress(dag_key2.pub())));
  ASSERT_TRUE(post_stats_with_get->getValidatorStats().contains(dev::toAddress(dag_key3.pub())));
  ASSERT_TRUE(post_stats_with_get->getValidatorStats().contains(dev::toAddress(dag_key5.pub())));
  ASSERT_EQ(post_stats_with_get->getValidatorStats().find(dev::toAddress(dag_key1.pub()))->second.dag_blocks_count_, 0);
  ASSERT_EQ(post_stats_with_get->getValidatorStats().find(dev::toAddress(dag_key2.pub()))->second.dag_blocks_count_, 0);
  ASSERT_EQ(post_stats_with_get->getValidatorStats().find(dev::toAddress(dag_key3.pub()))->second.dag_blocks_count_, 1);
  ASSERT_EQ(post_stats_with_get->getValidatorStats().find(dev::toAddress(dag_key5.pub()))->second.dag_blocks_count_, 1);

  ASSERT_EQ(stats_with_get->getValidatorStats().find(dev::toAddress(dag_key1.pub()))->second.fees_rewards_,
            post_stats_with_get->getValidatorStats().find(dev::toAddress(dag_key1.pub()))->second.fees_rewards_);
  ASSERT_EQ(stats_with_get->getValidatorStats().find(dev::toAddress(dag_key2.pub()))->second.fees_rewards_,
            post_stats_with_get->getValidatorStats().find(dev::toAddress(dag_key2.pub()))->second.fees_rewards_);
  ASSERT_EQ(stats_with_get->getValidatorStats().find(dev::toAddress(dag_key5.pub()))->second.fees_rewards_,
            post_stats_with_get->getValidatorStats().find(dev::toAddress(dag_key5.pub()))->second.fees_rewards_);
  ASSERT_EQ(post_stats_with_get->getValidatorStats().find(dev::toAddress(dag_key3.pub()))->second.fees_rewards_, 0);
}

}  // namespace taraxa::core_tests

using namespace taraxa;
int main(int argc, char** argv) {
  taraxa::static_init();

  auto logging = logger::createDefaultLoggingConfig();
  logging.verbosity = logger::Verbosity::Error;
  addr_t node_addr;
  logging.InitLogging(node_addr);

  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
