#include "rewards/rewards_stats.hpp"

#include <gtest/gtest.h>
#include <libdevcore/Address.h>
#include <libdevcore/Common.h>
#include <libdevcore/CommonJS.h>

#include "test_util/gtest.hpp"
#include "test_util/samples.hpp"

namespace taraxa::core_tests {

auto g_secret = dev::Secret("3800b2875669d9b2053c1aff9224ecfdc411423aac5b5a73d7a45ced1c3b9dcd",
                            dev::Secret::ConstructFromStringType::FromHex);
auto g_key_pair = Lazy([] { return dev::KeyPair(g_secret); });

struct RewardsStatsTest : NodesTest {};

class TestableRewardsStats : public rewards::Stats {
 public:
  TestableRewardsStats(const Hardforks::RewardsDistributionMap& rdm, std::shared_ptr<DB> db)
      : rewards::Stats(100, Hardforks{rdm, 0}, db, [](auto) { return 100; }) {}
  std::vector<rewards::BlockStats> getStats() { return blocks_stats_; }
};

class TestableBlockStats : public rewards::BlockStats {
 public:
  const addr_t& getAuthor() const { return block_author_; }
  const auto& getValidatorStats() const { return validators_stats_; }
};

TEST_F(RewardsStatsTest, defaultDistribution) {
  auto db = std::make_shared<DbStorage>(data_dir / "db");

  std::vector<std::shared_ptr<Vote>> empty_votes;
  auto rewards_stats = TestableRewardsStats({}, db);

  for (auto i = 1; i < 5; ++i) {
    PeriodData block(make_simple_pbft_block(blk_hash_t(i), i), empty_votes);
    auto stats = rewards_stats.processStats(block, {});
    ASSERT_EQ(stats.size(), 1);
    ASSERT_TRUE(rewards_stats.getStats().empty());
  }
}

TEST_F(RewardsStatsTest, statsSaving) {
  auto db = std::make_shared<DbStorage>(data_dir / "db");

  // distribute every 5 blocks
  Hardforks::RewardsDistributionMap distribution{{0, 5}};

  std::vector<std::shared_ptr<Vote>> empty_votes;
  std::vector<addr_t> block_authors;
  {
    auto rewards_stats = TestableRewardsStats(distribution, db);

    for (auto i = 1; i < 5; ++i) {
      auto kp = dev::KeyPair::create();
      block_authors.push_back(kp.address());

      PeriodData block(make_simple_pbft_block(blk_hash_t(i), i, kp.secret()), empty_votes);
      auto stats = rewards_stats.processStats(block, {});
      ASSERT_EQ(rewards_stats.getStats().size(), block_authors.size());
      ASSERT_TRUE(stats.empty());
    }
  }
  {
    // Load from db
    auto rewards_stats = TestableRewardsStats(distribution, db);
    auto stats = rewards_stats.getStats();
    ASSERT_EQ(rewards_stats.getStats().size(), block_authors.size());

    for (size_t i = 0; i < stats.size(); ++i) {
      auto stats_with_get = reinterpret_cast<TestableBlockStats*>(&stats[i]);
      ASSERT_EQ(stats_with_get->getAuthor(), block_authors[i]);
    }
  }
}

TEST_F(RewardsStatsTest, statsCleaning) {
  auto db = std::make_shared<DbStorage>(data_dir / "db");

  // distribute every 5 blocks
  Hardforks::RewardsDistributionMap distribution{{0, 5}};

  std::vector<std::shared_ptr<Vote>> empty_votes;
  std::vector<addr_t> block_authors;
  {
    auto rewards_stats = TestableRewardsStats(distribution, db);

    for (auto i = 1; i < 5; ++i) {
      auto kp = dev::KeyPair::create();
      block_authors.push_back(kp.address());

      PeriodData block(make_simple_pbft_block(blk_hash_t(i), i, kp.secret()), empty_votes);
      auto stats = rewards_stats.processStats(block, {});
      ASSERT_EQ(rewards_stats.getStats().size(), block_authors.size());
      ASSERT_TRUE(stats.empty());
    }

    // Process block 5 after which we should have no stats elements in db
    PeriodData block(make_simple_pbft_block(blk_hash_t(5), 5), empty_votes);
    rewards_stats.processStats(block, {});
  }

  // Load from db
  auto rewards_stats = TestableRewardsStats(distribution, db);
  ASSERT_TRUE(rewards_stats.getStats().empty());
}

TEST_F(RewardsStatsTest, statsProcessing) {
  auto db = std::make_shared<DbStorage>(data_dir / "db");
  // distribute every 10 blocks
  auto rewards_stats = TestableRewardsStats({{0, 10}}, db);

  std::vector<std::shared_ptr<Vote>> empty_votes;
  std::vector<addr_t> block_authors;

  // make blocks [1,9] and process them. output of processStats should be empty
  for (auto i = 1; i < 10; ++i) {
    auto kp = dev::KeyPair::create();
    block_authors.push_back(kp.address());

    PeriodData block(make_simple_pbft_block(blk_hash_t(i), i, kp.secret()), empty_votes);
    auto stats = rewards_stats.processStats(block, {});
    ASSERT_TRUE(stats.empty());
    ASSERT_EQ(rewards_stats.getStats().size(), block_authors.size());
  }

  auto kp = dev::KeyPair::create();
  block_authors.push_back(kp.address());

  PeriodData block(make_simple_pbft_block(blk_hash_t(10), 10, kp.secret()), empty_votes);
  auto stats = rewards_stats.processStats(block, {});
  ASSERT_EQ(stats.size(), block_authors.size());

  for (size_t i = 0; i < stats.size(); ++i) {
    auto stats_with_get = reinterpret_cast<TestableBlockStats*>(&stats[i]);
    ASSERT_EQ(stats_with_get->getAuthor(), block_authors[i]);
  }
  ASSERT_TRUE(rewards_stats.getStats().empty());
}

TEST_F(RewardsStatsTest, distributionChange) {
  auto db = std::make_shared<DbStorage>(data_dir / "db");

  Hardforks::RewardsDistributionMap distribution{{6, 5}, {11, 2}};

  auto rewards_stats = TestableRewardsStats(distribution, db);

  std::vector<std::shared_ptr<Vote>> empty_votes;
  uint64_t period = 1;
  for (; period <= 5; ++period) {
    PeriodData block(make_simple_pbft_block(blk_hash_t(period), period), empty_votes);
    auto stats = rewards_stats.processStats(block, {});
    ASSERT_FALSE(stats.empty());
  }
  {
    // make blocks [1,9] and process them. output of processStats should be empty
    for (; period < 10; ++period) {
      PeriodData block(make_simple_pbft_block(blk_hash_t(period), period), empty_votes);
      auto stats = rewards_stats.processStats(block, {});
      ASSERT_TRUE(stats.empty());
    }
    PeriodData block(make_simple_pbft_block(blk_hash_t(period), period), empty_votes);
    auto stats = rewards_stats.processStats(block, {});
  }

  PeriodData block(make_simple_pbft_block(blk_hash_t(period), period), empty_votes);
  auto stats = rewards_stats.processStats(block, {});
}

TEST_F(RewardsStatsTest, feeRewards) {
  auto db = std::make_shared<DbStorage>(data_dir / "db");
  auto pbft_proposer = dev::KeyPair::create();
  auto dag_proposer = dev::KeyPair::create();

  Hardforks::RewardsDistributionMap distribution{};

  auto rewards_stats = TestableRewardsStats(distribution, db);

  std::vector<std::shared_ptr<Vote>> empty_votes;
  uint64_t period = 1;
  uint64_t nonce = 1;

  const auto trx_gas_fee = 1000000;
  auto trx = std::make_shared<Transaction>(nonce++, 0, 1, trx_gas_fee, dev::fromHex(samples::greeter_contract_code),
                                           pbft_proposer.secret());

  DagBlock dag_blk({}, {}, {}, {trx->getHash()}, {}, {}, dag_proposer.secret());
  db->saveDagBlock(dag_blk);
  std::vector<vote_hash_t> reward_votes_hashes;
  auto pbft_block =
      std::make_shared<PbftBlock>(kNullBlockHash, kNullBlockHash, kNullBlockHash, kNullBlockHash, period,
                                  addr_t::random(), pbft_proposer.secret(), std::move(reward_votes_hashes));

  PeriodData period_data(pbft_block, empty_votes);
  period_data.dag_blocks.push_back(dag_blk);
  period_data.transactions = {trx};

  auto stats = rewards_stats.processStats(period_data, {trx_gas_fee}).front();

  auto testable_stats = reinterpret_cast<TestableBlockStats*>(&stats);
  auto validators_stats = testable_stats->getValidatorStats();
  // stats should be only for dag block proposer
  ASSERT_EQ(validators_stats.size(), 1);
  for (const auto& vs : validators_stats) {
    if (vs.first == dag_proposer.address()) {
      ASSERT_EQ(vs.second.dag_blocks_count_, 1);
      ASSERT_EQ(vs.second.fees_rewards_, trx_gas_fee);
    }
  }
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