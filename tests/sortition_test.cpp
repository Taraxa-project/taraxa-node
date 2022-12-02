#include <gtest/gtest.h>

#include "common/types.hpp"
#include "dag/sortition_params_manager.hpp"
#include "util_test/util.hpp"

namespace taraxa::core_tests {

struct SortitionTest : BaseTest {
  std::vector<FullNodeConfig> node_cfgs = make_node_cfgs(1);
};

vec_trx_t generateTrxHashes(size_t count) {
  vec_trx_t res;
  for (size_t i = 0; i < count; ++i) {
    res.push_back(dev::h256(i));
  }
  return res;
}

PeriodData createBlock(PbftPeriod period, uint16_t efficiency, size_t dag_blocks_count = 5,
                       blk_hash_t anchor_hash = blk_hash_t(1)) {
  // produced transactions count should be equal to or multiple of this value to produce block with accurate enough
  // efficiency
  const size_t kTrxCount = 100 * kOnePercent;
  PeriodData b;
  std::vector<vote_hash_t> reward_votes_hashes;
  b.pbft_blk = std::make_shared<PbftBlock>(blk_hash_t(), anchor_hash, blk_hash_t(), period, addr_t(0),
                                           dev::KeyPair::create().secret(), std::move(reward_votes_hashes));
  size_t effective_transactions = kTrxCount * efficiency / (100 * kOnePercent);
  auto trx_hashes = generateTrxHashes(effective_transactions);
  auto trx_per_block = effective_transactions / dag_blocks_count;

  for (uint32_t i = 0; i < trx_hashes.size(); ++i) {
    b.transactions.push_back(std::make_shared<Transaction>());
  }

  for (size_t i = 0; i < dag_blocks_count; ++i) {
    vec_trx_t trxs{trx_hashes.begin() + i * trx_per_block, trx_hashes.begin() + (i + 1) * trx_per_block};
    b.dag_blocks.push_back({{}, {}, {}, trxs, {}});
  };

  size_t issued_overlap_count = 0;
  while (issued_overlap_count != kTrxCount - effective_transactions) {
    size_t overlap = std::min(kTrxCount - effective_transactions - issued_overlap_count, trx_hashes.size());
    issued_overlap_count += overlap;
    vec_trx_t trxs{trx_hashes.begin(), trx_hashes.begin() + overlap};
    b.dag_blocks.push_back({{}, {}, {}, trxs, {}});
  }
  return b;
}

TEST_F(SortitionTest, vrf_lower_overflow) {
  VrfParams vrf;

  vrf.threshold_upper = 300;

  vrf += -200;

  EXPECT_EQ(vrf.threshold_upper, 100);

  vrf += -200;

  EXPECT_EQ(vrf.threshold_upper, VrfParams::kThresholdUpperMinValue);

  vrf += 50;

  EXPECT_EQ(vrf.threshold_upper, VrfParams::kThresholdUpperMinValue + 50);
}

TEST_F(SortitionTest, vrf_upper_overflow) {
  VrfParams vrf;

  vrf.threshold_upper = std::numeric_limits<uint16_t>::max() - 100;

  vrf += 200;

  EXPECT_EQ(vrf.threshold_upper, std::numeric_limits<uint16_t>::max());

  vrf += 200;

  EXPECT_EQ(vrf.threshold_upper, std::numeric_limits<uint16_t>::max());
}

TEST_F(SortitionTest, sortition_config_serialization) {
  // Fill with values that are different from default one, so take random
  SortitionConfig config;

  config.computation_interval = std::rand();
  config.dag_efficiency_targets = {std::rand(), std::rand()};

  config.vdf.difficulty_min = std::rand();
  config.vdf.difficulty_max = std::rand();
  config.vdf.difficulty_stale = std::rand();
  config.vdf.lambda_bound = std::rand();

  config.vrf.threshold_upper = std::rand();

  auto config_json = enc_json(config);
  SortitionConfig restored_config;
  dec_json(config_json, restored_config);

  // Raw compare because it is not needed anywhere else
  EXPECT_EQ(config.computation_interval, restored_config.computation_interval);
  EXPECT_EQ(config.dag_efficiency_targets, restored_config.dag_efficiency_targets);

  EXPECT_EQ(config.vdf.difficulty_min, restored_config.vdf.difficulty_min);
  EXPECT_EQ(config.vdf.difficulty_max, restored_config.vdf.difficulty_max);
  EXPECT_EQ(config.vdf.difficulty_stale, restored_config.vdf.difficulty_stale);
  EXPECT_EQ(config.vdf.lambda_bound, restored_config.vdf.lambda_bound);

  EXPECT_EQ(config.vrf.threshold_upper, restored_config.vrf.threshold_upper);
}

TEST_F(SortitionTest, params_change_serialization) {
  SortitionParamsChange start(1, 25 * kOnePercent, {1100});
  // 1700 - 1500 / 2 = 100 (change per percent)
  SortitionParamsChange params(2, 27 * kOnePercent, {1300});

  const auto params_rlp = params.rlp();
  const auto deserialized_params = SortitionParamsChange::from_rlp(dev::RLP(params_rlp));

  EXPECT_EQ(params.vrf_params.threshold_upper, deserialized_params.vrf_params.threshold_upper);
  EXPECT_EQ(params.interval_efficiency, deserialized_params.interval_efficiency);
}

TEST_F(SortitionTest, efficiency_calculation) {
  size_t tries = 10;
  auto db = std::make_shared<DbStorage>(data_dir / "db");
  SortitionParamsManager sp({}, node_cfgs[0].genesis.sortition, db);

  while (tries--) {
    auto target_efficiency = std::rand() % 100 * kOnePercent;
    auto b = createBlock(1, target_efficiency, 5);

    auto calc_efficiency = sp.calculateDagEfficiency(b);
    EXPECT_EQ(target_efficiency, calc_efficiency);
  }
}

TEST_F(SortitionTest, db_keys_order) {
  auto db = std::make_shared<DbStorage>(data_dir / "db");

  auto batch = db->createWriteBatch();
  for (uint16_t i = 1; i <= 1000; ++i) {
    db->saveSortitionParamsChange(i, {i, i, {}}, batch);
  }
  db->commitWriteBatch(batch);

  const auto params_from_db = db->getLastSortitionParams(1000);
  for (uint16_t i = 1; i < 1000; ++i) {
    EXPECT_EQ(i + 1, params_from_db[i].period);
  }
}

TEST_F(SortitionTest, params_changes_from_db) {
  auto db = std::make_shared<DbStorage>(data_dir / "db");

  auto batch = db->createWriteBatch();
  for (uint16_t i = 0; i < 10; ++i) {
    SortitionParamsChange p{i, i, {i}};
    db->saveSortitionParamsChange(i, p, batch);
  }
  db->commitWriteBatch(batch);

  auto res = db->getLastSortitionParams(5);
  EXPECT_EQ(res.size(), 5);
  for (uint16_t i = 0; i < 5; ++i) {
    // +5 is offset to the middle of data
    EXPECT_EQ(res[i].interval_efficiency, i + 5);
    EXPECT_EQ(res[i].vrf_params.threshold_upper, i + 5);
  }
}

TEST_F(SortitionTest, params_changes_from_db2) {
  auto db = std::make_shared<DbStorage>(data_dir / "db");

  auto batch = db->createWriteBatch();
  for (uint16_t i = 0; i < 2; ++i) {
    SortitionParamsChange p{i, i, {i}};
    db->saveSortitionParamsChange(i, p, batch);
  }
  db->commitWriteBatch(batch);

  auto res = db->getLastSortitionParams(5);
  EXPECT_EQ(res.size(), 2);
  for (uint16_t i = 0; i < 2; ++i) {
    EXPECT_EQ(res[i].interval_efficiency, i);
    EXPECT_EQ(res[i].vrf_params.threshold_upper, i);
  }
}

TEST_F(SortitionTest, db_cleanup) {
  auto& cfg = node_cfgs[0].genesis.sortition;
  cfg.computation_interval = 5;
  cfg.changing_interval = 5;
  cfg.dag_efficiency_targets = {75 * kOnePercent, 75 * kOnePercent};

  auto db = std::make_shared<DbStorage>(data_dir / "db");
  SortitionParamsManager sp({}, node_cfgs[0].genesis.sortition, db);

  {
    auto batch = db->createWriteBatch();
    for (int i = 1; i <= 2 * cfg.computation_interval * cfg.changes_count_for_average; i++) {
      auto efficiency = 72 * kOnePercent;
      auto b = createBlock(i, efficiency, 5);
      sp.pbftBlockPushed(b, batch, b.pbft_blk->getPeriod());
    }
    db->commitWriteBatch(batch);
    EXPECT_EQ(db->getLastSortitionParams(cfg.changes_count_for_average).size(), 10);
  }
  {
    auto batch = db->createWriteBatch();
    for (int i = 1; i <= 3; i++) {
      auto efficiency = 73 * kOnePercent;
      auto b = createBlock(2 * cfg.computation_interval * cfg.changes_count_for_average + i, efficiency, 5);
      sp.pbftBlockPushed(b, batch, b.pbft_blk->getPeriod());
    }
    db->commitWriteBatch(batch);
    EXPECT_EQ(db->getLastSortitionParams(cfg.changes_count_for_average).size(), 10);
  }
}

TEST_F(SortitionTest, get_params_from_period) {
  auto& cfg = node_cfgs[0].genesis.sortition;
  cfg.changing_interval = 10;
  cfg.computation_interval = 10;
  cfg.dag_efficiency_targets = {48 * kOnePercent, 52 * kOnePercent};

  {
    auto db = std::make_shared<DbStorage>(data_dir / "db");
    SortitionParamsManager sp({}, node_cfgs[0].genesis.sortition, db);
    auto batch = db->createWriteBatch();
    {
      auto b = createBlock(10, 25 * kOnePercent);
      sp.pbftBlockPushed(b, batch, b.pbft_blk->getPeriod());
      db->commitWriteBatch(batch);
    }
    {
      auto b = createBlock(20, 35 * kOnePercent);
      sp.pbftBlockPushed(b, batch, b.pbft_blk->getPeriod());
      db->commitWriteBatch(batch);
    }
    {
      auto b = createBlock(40, 45 * kOnePercent);
      sp.pbftBlockPushed(b, batch, b.pbft_blk->getPeriod());
      db->commitWriteBatch(batch);
    }
    {
      auto b = createBlock(50, 53 * kOnePercent);
      sp.pbftBlockPushed(b, batch, b.pbft_blk->getPeriod());
      db->commitWriteBatch(batch);
    }
    {
      auto b = createBlock(60, 47 * kOnePercent);
      sp.pbftBlockPushed(b, batch, b.pbft_blk->getPeriod());
      db->commitWriteBatch(batch);
    }
    {
      auto b = createBlock(70, 48 * kOnePercent);
      sp.pbftBlockPushed(b, batch, b.pbft_blk->getPeriod());
      db->commitWriteBatch(batch);
    }
    {
      auto b = createBlock(80, 52 * kOnePercent);
      sp.pbftBlockPushed(b, batch, b.pbft_blk->getPeriod());
      db->commitWriteBatch(batch);
    }

    const auto params_for_period_10_19 = cfg.vrf.threshold_upper - UINT16_MAX / 20;
    const auto params_for_period_20_39 = params_for_period_10_19 - UINT16_MAX / 50;
    const auto params_for_period_40_49 = params_for_period_20_39 - params_for_period_20_39 / 100;
    const auto params_for_period_50_59 = (params_for_period_20_39 + params_for_period_40_49) / 2;
    const auto params_for_period_60_and_more = (params_for_period_40_49 + params_for_period_50_59) / 2;
    EXPECT_EQ(sp.getSortitionParams(11).vrf.threshold_upper, params_for_period_10_19);
    EXPECT_EQ(sp.getSortitionParams(19).vrf.threshold_upper, params_for_period_10_19);
    EXPECT_EQ(sp.getSortitionParams(21).vrf.threshold_upper, params_for_period_20_39);
    EXPECT_EQ(sp.getSortitionParams(29).vrf.threshold_upper, params_for_period_20_39);
    EXPECT_EQ(sp.getSortitionParams(32).vrf.threshold_upper, params_for_period_20_39);
    EXPECT_EQ(sp.getSortitionParams(39).vrf.threshold_upper, params_for_period_20_39);
    EXPECT_EQ(sp.getSortitionParams(41).vrf.threshold_upper, params_for_period_40_49);
    EXPECT_EQ(sp.getSortitionParams(51).vrf.threshold_upper, params_for_period_50_59);
    EXPECT_EQ(sp.getSortitionParams(59).vrf.threshold_upper, params_for_period_50_59);
    EXPECT_EQ(sp.getSortitionParams(69).vrf.threshold_upper, params_for_period_60_and_more);
    EXPECT_EQ(sp.getSortitionParams(79).vrf.threshold_upper, params_for_period_60_and_more);
    EXPECT_EQ(sp.getSortitionParams(210).vrf.threshold_upper, params_for_period_60_and_more);

    EXPECT_EQ(db->getParamsChangeForPeriod(11)->vrf_params.threshold_upper, params_for_period_10_19);
    EXPECT_EQ(db->getParamsChangeForPeriod(19)->vrf_params.threshold_upper, params_for_period_10_19);
    EXPECT_EQ(db->getParamsChangeForPeriod(21)->vrf_params.threshold_upper, params_for_period_20_39);
    EXPECT_EQ(db->getParamsChangeForPeriod(29)->vrf_params.threshold_upper, params_for_period_20_39);
    EXPECT_EQ(db->getParamsChangeForPeriod(32)->vrf_params.threshold_upper, params_for_period_20_39);
    EXPECT_EQ(db->getParamsChangeForPeriod(39)->vrf_params.threshold_upper, params_for_period_20_39);
    EXPECT_EQ(db->getParamsChangeForPeriod(41)->vrf_params.threshold_upper, params_for_period_40_49);
    EXPECT_EQ(db->getParamsChangeForPeriod(51)->vrf_params.threshold_upper, params_for_period_50_59);
    EXPECT_EQ(db->getParamsChangeForPeriod(59)->vrf_params.threshold_upper, params_for_period_50_59);
    EXPECT_EQ(db->getParamsChangeForPeriod(69)->vrf_params.threshold_upper, params_for_period_60_and_more);
    EXPECT_EQ(db->getParamsChangeForPeriod(79)->vrf_params.threshold_upper, params_for_period_60_and_more);
    EXPECT_EQ(db->getParamsChangeForPeriod(210)->vrf_params.threshold_upper, params_for_period_60_and_more);
  }
}

TEST_F(SortitionTest, get_params_from_period_reverse) {
  auto& cfg = node_cfgs[0].genesis.sortition;
  cfg.changing_interval = 10;
  cfg.computation_interval = 10;
  cfg.dag_efficiency_targets = {48 * kOnePercent, 52 * kOnePercent};

  {
    auto db = std::make_shared<DbStorage>(data_dir / "db");
    SortitionParamsManager sp({}, node_cfgs[0].genesis.sortition, db);
    auto batch = db->createWriteBatch();
    {
      auto b = createBlock(10, 75 * kOnePercent);
      sp.pbftBlockPushed(b, batch, b.pbft_blk->getPeriod());
      db->commitWriteBatch(batch);
    }
    {
      auto b = createBlock(20, 65 * kOnePercent);
      sp.pbftBlockPushed(b, batch, b.pbft_blk->getPeriod());
      db->commitWriteBatch(batch);
    }
    {
      auto b = createBlock(40, 55 * kOnePercent);
      sp.pbftBlockPushed(b, batch, b.pbft_blk->getPeriod());
      db->commitWriteBatch(batch);
    }
    {
      auto b = createBlock(50, 47 * kOnePercent);
      sp.pbftBlockPushed(b, batch, b.pbft_blk->getPeriod());
      db->commitWriteBatch(batch);
    }
    {
      auto b = createBlock(60, 53 * kOnePercent);
      sp.pbftBlockPushed(b, batch, b.pbft_blk->getPeriod());
      db->commitWriteBatch(batch);
    }

    const auto params_for_period_10_19 = cfg.vrf.threshold_upper + UINT16_MAX / 20;
    const auto params_for_period_20_39 = params_for_period_10_19 + UINT16_MAX / 50;
    const auto params_for_period_40_49 = params_for_period_20_39 + params_for_period_20_39 / 100;
    const auto params_for_period_50_59 = (params_for_period_20_39 + params_for_period_40_49) / 2;
    const auto params_for_period_60_and_more = (params_for_period_40_49 + params_for_period_50_59) / 2;
    EXPECT_EQ(sp.getSortitionParams(11).vrf.threshold_upper, params_for_period_10_19);
    EXPECT_EQ(sp.getSortitionParams(19).vrf.threshold_upper, params_for_period_10_19);
    EXPECT_EQ(sp.getSortitionParams(21).vrf.threshold_upper, params_for_period_20_39);
    EXPECT_EQ(sp.getSortitionParams(29).vrf.threshold_upper, params_for_period_20_39);
    EXPECT_EQ(sp.getSortitionParams(32).vrf.threshold_upper, params_for_period_20_39);
    EXPECT_EQ(sp.getSortitionParams(39).vrf.threshold_upper, params_for_period_20_39);
    EXPECT_EQ(sp.getSortitionParams(41).vrf.threshold_upper, params_for_period_40_49);
    EXPECT_EQ(sp.getSortitionParams(51).vrf.threshold_upper, params_for_period_50_59);
    EXPECT_EQ(sp.getSortitionParams(59).vrf.threshold_upper, params_for_period_50_59);
    EXPECT_EQ(sp.getSortitionParams(69).vrf.threshold_upper, params_for_period_60_and_more);
    EXPECT_EQ(sp.getSortitionParams(79).vrf.threshold_upper, params_for_period_60_and_more);
    EXPECT_EQ(sp.getSortitionParams(210).vrf.threshold_upper, params_for_period_60_and_more);

    EXPECT_EQ(db->getParamsChangeForPeriod(11)->vrf_params.threshold_upper, params_for_period_10_19);
    EXPECT_EQ(db->getParamsChangeForPeriod(19)->vrf_params.threshold_upper, params_for_period_10_19);
    EXPECT_EQ(db->getParamsChangeForPeriod(21)->vrf_params.threshold_upper, params_for_period_20_39);
    EXPECT_EQ(db->getParamsChangeForPeriod(29)->vrf_params.threshold_upper, params_for_period_20_39);
    EXPECT_EQ(db->getParamsChangeForPeriod(32)->vrf_params.threshold_upper, params_for_period_20_39);
    EXPECT_EQ(db->getParamsChangeForPeriod(39)->vrf_params.threshold_upper, params_for_period_20_39);
    EXPECT_EQ(db->getParamsChangeForPeriod(41)->vrf_params.threshold_upper, params_for_period_40_49);
    EXPECT_EQ(db->getParamsChangeForPeriod(51)->vrf_params.threshold_upper, params_for_period_50_59);
    EXPECT_EQ(db->getParamsChangeForPeriod(59)->vrf_params.threshold_upper, params_for_period_50_59);
    EXPECT_EQ(db->getParamsChangeForPeriod(69)->vrf_params.threshold_upper, params_for_period_60_and_more);
    EXPECT_EQ(db->getParamsChangeForPeriod(79)->vrf_params.threshold_upper, params_for_period_60_and_more);
    EXPECT_EQ(db->getParamsChangeForPeriod(210)->vrf_params.threshold_upper, params_for_period_60_and_more);
  }
}

TEST_F(SortitionTest, efficiency_restart) {
  // Test verifies that the efficiency memory structure contains same values before and after node restart
  auto& cfg = node_cfgs[0].genesis.sortition;
  cfg.changing_interval = 6;
  cfg.computation_interval = 3;
  cfg.dag_efficiency_targets = {48 * kOnePercent, 52 * kOnePercent};

  {
    auto db = std::make_shared<DbStorage>(data_dir / "db");
    SortitionParamsManager sp({}, node_cfgs[0].genesis.sortition, db);
    auto batch = db->createWriteBatch();
    {
      auto b = createBlock(1, 75 * kOnePercent);
      sp.pbftBlockPushed(b, batch, 1);
      db->savePeriodData(b, batch);

      b = createBlock(2, 74 * kOnePercent);
      sp.pbftBlockPushed(b, batch, 2);
      db->savePeriodData(b, batch);

      // Empty block to be ignored
      b = createBlock(3, 74 * kOnePercent, 5, {});
      db->savePeriodData(b, batch);

      b = createBlock(4, 73 * kOnePercent);
      sp.pbftBlockPushed(b, batch, 3);
      db->savePeriodData(b, batch);

      // Empty block to be ignored
      b = createBlock(5, 74 * kOnePercent, 5, {});
      db->savePeriodData(b, batch);

      b = createBlock(6, 50 * kOnePercent);
      sp.pbftBlockPushed(b, batch, 4);
      db->savePeriodData(b, batch);
      db->commitWriteBatch(batch);
    }
    EXPECT_EQ(sp.averageDagEfficiency(), 50 * kOnePercent);
  }

  {
    auto db = std::make_shared<DbStorage>(data_dir / "db");
    auto batch = db->createWriteBatch();
    SortitionParamsManager sp({}, node_cfgs[0].genesis.sortition, db);
    EXPECT_EQ(sp.averageDagEfficiency(), 50 * kOnePercent);

    // Empty block to be ignored
    auto b = createBlock(7, 74 * kOnePercent, 5, {});
    db->savePeriodData(b, batch);

    b = createBlock(8, 40 * kOnePercent);
    sp.pbftBlockPushed(b, batch, 5);
    db->savePeriodData(b, batch);
    db->commitWriteBatch(batch);

    EXPECT_EQ(sp.averageDagEfficiency(), 45 * kOnePercent);
  }

  {
    auto db = std::make_shared<DbStorage>(data_dir / "db");
    auto batch = db->createWriteBatch();
    SortitionParamsManager sp({}, node_cfgs[0].genesis.sortition, db);
    EXPECT_EQ(sp.averageDagEfficiency(), 45 * kOnePercent);

    // Empty block to be ignored
    auto b = createBlock(9, 74 * kOnePercent, 5, {});
    db->savePeriodData(b, batch);

    b = createBlock(10, 30 * kOnePercent);
    sp.pbftBlockPushed(b, batch, 6);
    db->savePeriodData(b, batch);
    db->commitWriteBatch(batch);

    b = createBlock(11, 30 * kOnePercent);
    sp.pbftBlockPushed(b, batch, 7);
    db->savePeriodData(b, batch);
    db->commitWriteBatch(batch);

    b = createBlock(12, 30 * kOnePercent);
    sp.pbftBlockPushed(b, batch, 8);
    db->savePeriodData(b, batch);
    db->commitWriteBatch(batch);

    b = createBlock(13, 30 * kOnePercent);
    sp.pbftBlockPushed(b, batch, 9);
    db->savePeriodData(b, batch);
    db->commitWriteBatch(batch);

    // Empty block to be ignored
    b = createBlock(14, 74 * kOnePercent, 5, {});
    db->savePeriodData(b, batch);

    b = createBlock(15, 20 * kOnePercent);
    sp.pbftBlockPushed(b, batch, 10);
    db->savePeriodData(b, batch);
    db->commitWriteBatch(batch);

    b = createBlock(16, 10 * kOnePercent);
    sp.pbftBlockPushed(b, batch, 11);
    db->savePeriodData(b, batch);
    db->commitWriteBatch(batch);

    EXPECT_EQ(sp.averageDagEfficiency(), 15 * kOnePercent);
  }

  {
    auto db = std::make_shared<DbStorage>(data_dir / "db");
    auto batch = db->createWriteBatch();
    SortitionParamsManager sp({}, node_cfgs[0].genesis.sortition, db);
    EXPECT_EQ(sp.averageDagEfficiency(), 15 * kOnePercent);
  }
}

TEST_F(SortitionTest, params_restart) {
  // Test verifies that params_changes memory structure contains same values before and after node restart
  auto& cfg = node_cfgs[0].genesis.sortition;
  cfg.changes_count_for_average = 3;
  cfg.changing_interval = 1;
  cfg.computation_interval = 1;
  {
    auto db = std::make_shared<DbStorage>(data_dir / "db");
    SortitionParamsManager sp({}, node_cfgs[0].genesis.sortition, db);
    auto params_changes = sp.getParamsChanges();
    EXPECT_EQ(params_changes.size(), 1);
    EXPECT_EQ(params_changes[0].period, 0);
    EXPECT_EQ(params_changes[0].vrf_params.threshold_upper, cfg.vrf.threshold_upper);
    auto batch = db->createWriteBatch();
    auto b = createBlock(1, 75 * kOnePercent);
    sp.pbftBlockPushed(b, batch, 1);
    db->savePeriodData(b, batch);
    db->commitWriteBatch(batch);
    params_changes = sp.getParamsChanges();
    EXPECT_EQ(params_changes.size(), 2);
    b = createBlock(2, 65 * kOnePercent);
    sp.pbftBlockPushed(b, batch, 2);
    db->savePeriodData(b, batch);
    db->commitWriteBatch(batch);
    params_changes = sp.getParamsChanges();
    EXPECT_EQ(params_changes.size(), 3);
    b = createBlock(3, 55 * kOnePercent);
    sp.pbftBlockPushed(b, batch, 2);
    db->savePeriodData(b, batch);
    db->commitWriteBatch(batch);
    params_changes = sp.getParamsChanges();
    EXPECT_EQ(params_changes.size(), 3);
    EXPECT_EQ(params_changes[0].period, 1);
    EXPECT_EQ(params_changes[1].period, 2);
    EXPECT_EQ(params_changes[2].period, 3);
  }
  {
    auto db = std::make_shared<DbStorage>(data_dir / "db");
    SortitionParamsManager sp({}, node_cfgs[0].genesis.sortition, db);
    auto params_changes = sp.getParamsChanges();
    EXPECT_EQ(params_changes.size(), 3);
    EXPECT_EQ(params_changes[0].period, 1);
    EXPECT_EQ(params_changes[1].period, 2);
    EXPECT_EQ(params_changes[2].period, 3);
  }
}

}  // namespace taraxa::core_tests