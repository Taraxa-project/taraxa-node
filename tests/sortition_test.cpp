#include <gtest/gtest.h>

#include "common/types.hpp"
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

SyncBlock createBlock(uint64_t period, uint16_t efficiency, size_t dag_blocks_count = 5) {
  // produced transactions count should be equal to or multiple of this value to produce block with accurate enough
  // efficiency
  const size_t kTrxCount = 100 * kOnePercent;
  SyncBlock b;
  auto pbft_blk = std::make_shared<PbftBlock>(PbftBlock{{}, {}, {}, period, {}, dev::KeyPair::create().secret()});
  size_t effective_transactions = kTrxCount * efficiency / (100 * kOnePercent);
  auto trx_hashes = generateTrxHashes(effective_transactions);
  auto trx_per_block = effective_transactions / dag_blocks_count;

  std::vector<Transaction> transactions;
  transactions.reserve(trx_hashes.size());
  for (uint32_t i = 0; i < trx_hashes.size(); ++i) {
    transactions.push_back(Transaction());
  }

  std::vector<DagBlock> dag_blocks;
  dag_blocks.reserve(dag_blocks_count);
  for (size_t i = 0; i < dag_blocks_count; ++i) {
    vec_trx_t trxs{trx_hashes.begin() + i * trx_per_block, trx_hashes.begin() + (i + 1) * trx_per_block};
    dag_blocks.push_back({{}, 0, {}, std::move(trxs), {}, {}, addr_t(0)});
  };

  size_t issued_overlap_count = 0;
  while (issued_overlap_count != kTrxCount - effective_transactions) {
    size_t overlap = std::min(kTrxCount - effective_transactions - issued_overlap_count, trx_hashes.size());
    issued_overlap_count += overlap;
    vec_trx_t trxs{trx_hashes.begin(), trx_hashes.begin() + overlap};
    dag_blocks.push_back({{}, 0, {}, std::move(trxs), {}, {}, addr_t(0)});
  }

  return SyncBlock(std::move(pbft_blk), std::move(dag_blocks), {}, std::move(transactions), {});
}

TEST_F(SortitionTest, vrf_lower_overflow) {
  VrfParams vrf;

  vrf.threshold_range = 200;
  vrf.threshold_upper = 300;

  vrf += -200;

  EXPECT_EQ(vrf.threshold_range, 200);
  EXPECT_EQ(vrf.threshold_upper, 100);

  vrf += -200;

  EXPECT_EQ(vrf.threshold_range, 200);
  EXPECT_EQ(vrf.threshold_upper, VrfParams::kThresholdUpperMinValue);

  vrf += 50;

  EXPECT_EQ(vrf.threshold_range, 200);
  EXPECT_EQ(vrf.threshold_upper, VrfParams::kThresholdUpperMinValue + 50);
}

TEST_F(SortitionTest, vrf_upper_overflow) {
  VrfParams vrf;

  vrf.threshold_upper = std::numeric_limits<uint16_t>::max() - 100;
  vrf.threshold_range = 200;

  vrf += 200;

  EXPECT_EQ(vrf.threshold_upper, std::numeric_limits<uint16_t>::max());
  EXPECT_EQ(vrf.threshold_range, 200);

  vrf += 200;

  EXPECT_EQ(vrf.threshold_upper, std::numeric_limits<uint16_t>::max());
  EXPECT_EQ(vrf.threshold_range, 200);
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

  config.vrf.threshold_range = std::rand();
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

  EXPECT_EQ(config.vrf.threshold_range, restored_config.vrf.threshold_range);
  EXPECT_EQ(config.vrf.threshold_upper, restored_config.vrf.threshold_upper);
}

TEST_F(SortitionTest, params_change_serialization) {
  SortitionParamsChange start(1, 25 * kOnePercent, {1100, 1500});
  // 1700 - 1500 / 2 = 100 (change per percent)
  SortitionParamsChange params(2, 27 * kOnePercent, {1300, 1700}, start);

  EXPECT_EQ(params.actual_correction_per_percent, 100);

  const auto params_rlp = params.rlp();
  const auto deserialized_params = SortitionParamsChange::from_rlp(dev::RLP(params_rlp));

  EXPECT_EQ(params.vrf_params.threshold_range, deserialized_params.vrf_params.threshold_range);
  EXPECT_EQ(params.vrf_params.threshold_upper, deserialized_params.vrf_params.threshold_upper);
  EXPECT_EQ(params.interval_efficiency, deserialized_params.interval_efficiency);
  EXPECT_EQ(params.actual_correction_per_percent, deserialized_params.actual_correction_per_percent);
}

TEST_F(SortitionTest, efficiency_calculation) {
  size_t tries = 10;
  auto db = std::make_shared<DbStorage>(data_dir / "db");
  SortitionParamsManager sp({}, node_cfgs[0].chain.sortition, db);

  while (tries--) {
    auto target_efficiency = std::rand() % 100 * kOnePercent;
    auto b = createBlock(1, target_efficiency, 5);

    auto calc_efficiency = sp.calculateDagEfficiency(b);
    EXPECT_EQ(target_efficiency, calc_efficiency);
  }
}

TEST_F(SortitionTest, correction_calculation) {
  VrfParams params{300, 100};

  SortitionParamsChange pc1(1, 60 * kOnePercent, params);
  SortitionParamsChange pc2(2, 62 * kOnePercent, {400, 200}, pc1);

  // 100 / 2 = 50
  EXPECT_EQ(pc2.actual_correction_per_percent, 50);
}

TEST_F(SortitionTest, minimal_correction) {
  VrfParams params{100, 300};

  SortitionParamsChange pc1(1, 60 * kOnePercent, params);
  SortitionParamsChange pc2(2, 62 * kOnePercent, params, pc1);

  // params not changed so correction should be minimal(1)
  EXPECT_EQ(pc2.actual_correction_per_percent, 1);
}

TEST_F(SortitionTest, average_correction_per_percent) {
  node_cfgs[0].chain.sortition.computation_interval = 5;
  node_cfgs[0].chain.sortition.dag_efficiency_targets = {75 * kOnePercent, 75 * kOnePercent};

  auto db = std::make_shared<DbStorage>(data_dir / "db");
  SortitionParamsManager sp({}, node_cfgs[0].chain.sortition, db);

  auto threshold_upper = sp.getSortitionParams().vrf.threshold_upper;
  auto correction_per_percent = sp.averageCorrectionPerPercent();
  {
    auto batch = db->createWriteBatch();
    for (int i = 1; i <= 5; i++) {
      auto efficiency = 72 * kOnePercent;
      auto b = createBlock(i, efficiency, 5);
      sp.pbftBlockPushed(b, batch);
    }
    db->commitWriteBatch(batch);
  }
  EXPECT_EQ(correction_per_percent, 1);
  EXPECT_EQ(threshold_upper -= 3 * correction_per_percent, sp.getSortitionParams().vrf.threshold_upper);
  threshold_upper = sp.getSortitionParams().vrf.threshold_upper;
  correction_per_percent = sp.averageCorrectionPerPercent();
  EXPECT_EQ(correction_per_percent, 1);

  {
    auto batch = db->createWriteBatch();
    for (int i = 6; i <= 10; i++) {
      auto efficiency = 72 * kOnePercent;
      auto b = createBlock(i, efficiency, 5);
      sp.pbftBlockPushed(b, batch);
    }
    db->commitWriteBatch(batch);
  }

  EXPECT_EQ(threshold_upper -= 3 * correction_per_percent, sp.getSortitionParams().vrf.threshold_upper);
  threshold_upper = sp.getSortitionParams().vrf.threshold_upper;
  correction_per_percent = sp.averageCorrectionPerPercent();
  EXPECT_EQ(correction_per_percent, 150);

  {
    auto batch = db->createWriteBatch();
    for (int i = 11; i <= 15; i++) {
      auto efficiency = 77 * kOnePercent;
      auto b = createBlock(i, efficiency, 5);
      sp.pbftBlockPushed(b, batch);
    }
    db->commitWriteBatch(batch);
  }
  // 1 + 300 / 2 = 150
  EXPECT_EQ(threshold_upper += correction_per_percent * 2, sp.getSortitionParams().vrf.threshold_upper);
  threshold_upper = sp.getSortitionParams().vrf.threshold_upper;
  correction_per_percent = sp.averageCorrectionPerPercent();
  EXPECT_EQ(correction_per_percent, 120);

  {
    auto batch = db->createWriteBatch();
    for (int i = 16; i <= 20; i++) {
      auto efficiency = 74 * kOnePercent;
      auto b = createBlock(i, efficiency, 5);
      sp.pbftBlockPushed(b, batch);
    }
    db->commitWriteBatch(batch);
  }
  EXPECT_EQ(threshold_upper -= correction_per_percent * 1, sp.getSortitionParams().vrf.threshold_upper);

  {
    auto batch = db->createWriteBatch();
    for (int i = 21; i <= 23; i++) {
      auto efficiency = std::rand() % 100 * kOnePercent;
      auto b = createBlock(i, efficiency, 5);
      sp.pbftBlockPushed(b, batch);
    }
    db->commitWriteBatch(batch);
  }

  SortitionParamsManager sp2({}, node_cfgs[0].chain.sortition, db);
  EXPECT_EQ(sp2.averageDagEfficiency(), sp.averageDagEfficiency());
  EXPECT_EQ(sp2.averageCorrectionPerPercent(), sp.averageCorrectionPerPercent());
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
    SortitionParamsChange p{i, i, {i, i}};
    db->saveSortitionParamsChange(i, p, batch);
  }
  db->commitWriteBatch(batch);

  auto res = db->getLastSortitionParams(5);
  EXPECT_EQ(res.size(), 5);
  for (uint16_t i = 0; i < 5; ++i) {
    // +5 is offset to the middle of data
    EXPECT_EQ(res[i].interval_efficiency, i + 5);
    EXPECT_EQ(res[i].vrf_params.threshold_range, i + 5);
    EXPECT_EQ(res[i].vrf_params.threshold_upper, i + 5);
  }
}

TEST_F(SortitionTest, params_changes_from_db2) {
  auto db = std::make_shared<DbStorage>(data_dir / "db");

  auto batch = db->createWriteBatch();
  for (uint16_t i = 0; i < 2; ++i) {
    SortitionParamsChange p{i, i, {i, i}};
    db->saveSortitionParamsChange(i, p, batch);
  }
  db->commitWriteBatch(batch);

  auto res = db->getLastSortitionParams(5);
  EXPECT_EQ(res.size(), 2);
  for (uint16_t i = 0; i < 2; ++i) {
    EXPECT_EQ(res[i].interval_efficiency, i);
    EXPECT_EQ(res[i].vrf_params.threshold_range, i);
    EXPECT_EQ(res[i].vrf_params.threshold_upper, i);
  }
}

TEST_F(SortitionTest, efficiencies_from_db) {
  auto db = std::make_shared<DbStorage>(data_dir / "db");

  auto batch = db->createWriteBatch();
  for (int i = 0; i <= 143; ++i) {
    db->savePbftBlockDagEfficiency(i, i, batch);
  }
  db->commitWriteBatch(batch);

  const auto efficiencies = db->getLastIntervalEfficiencies(50);
  EXPECT_EQ(efficiencies.size(), 43);
  EXPECT_EQ(efficiencies.back(), 101);
  EXPECT_EQ(efficiencies.front(), 143);
}

TEST_F(SortitionTest, load_from_db) {
  auto db = std::make_shared<DbStorage>(data_dir / "db");

  auto batch = db->createWriteBatch();
  for (uint16_t i = 0; i < 10; ++i) {
    SortitionParamsChange p{i, i, {i, i}};
    p.actual_correction_per_percent = 25 * kOnePercent;
    db->saveSortitionParamsChange(i, p, batch);
  }
  for (int i = 0; i <= node_cfgs[0].chain.sortition.computation_interval - 1; ++i) {
    db->savePbftBlockDagEfficiency(i, 44 * kOnePercent, batch);
  }
  db->commitWriteBatch(batch);

  SortitionParamsManager sp({}, node_cfgs[0].chain.sortition, db);

  EXPECT_EQ(sp.averageCorrectionPerPercent(), 25 * kOnePercent);
  EXPECT_EQ(sp.averageDagEfficiency(), 44 * kOnePercent);
}

TEST_F(SortitionTest, db_cleanup) {
  auto& cfg = node_cfgs[0].chain.sortition;
  cfg.computation_interval = 5;
  cfg.dag_efficiency_targets = {75 * kOnePercent, 75 * kOnePercent};

  auto db = std::make_shared<DbStorage>(data_dir / "db");
  SortitionParamsManager sp({}, node_cfgs[0].chain.sortition, db);

  {
    auto batch = db->createWriteBatch();
    for (int i = 1; i <= 2 * cfg.computation_interval * cfg.changes_count_for_average; i++) {
      auto efficiency = 72 * kOnePercent;
      auto b = createBlock(i, efficiency, 5);
      sp.pbftBlockPushed(b, batch);
    }
    db->commitWriteBatch(batch);
    EXPECT_EQ(db->getLastIntervalEfficiencies(cfg.computation_interval).size(), 0);
    EXPECT_EQ(db->getLastSortitionParams(cfg.changes_count_for_average).size(), 5);
  }
  {
    auto batch = db->createWriteBatch();
    for (int i = 1; i <= 3; i++) {
      auto efficiency = 73 * kOnePercent;
      auto b = createBlock(2 * cfg.computation_interval * cfg.changes_count_for_average + i, efficiency, 5);
      sp.pbftBlockPushed(b, batch);
    }
    db->commitWriteBatch(batch);
    EXPECT_EQ(db->getLastIntervalEfficiencies(cfg.computation_interval).size(), 3);
    EXPECT_EQ(db->getLastSortitionParams(cfg.changes_count_for_average).size(), 5);
  }
}

TEST_F(SortitionTest, get_params_from_period) {
  auto& cfg = node_cfgs[0].chain.sortition;
  cfg.computation_interval = 10;
  cfg.dag_efficiency_targets = {75 * kOnePercent, 75 * kOnePercent};

  auto db = std::make_shared<DbStorage>(data_dir / "db");
  SortitionParamsManager sp({}, node_cfgs[0].chain.sortition, db);
  auto batch = db->createWriteBatch();
  {
    auto b = createBlock(10, 70 * kOnePercent, 5);
    sp.pbftBlockPushed(b, batch);
  }
  {
    auto b = createBlock(20, 70 * kOnePercent, 5);
    sp.pbftBlockPushed(b, batch);
  }
  {
    auto b = createBlock(30, 75 * kOnePercent, 5);
    sp.pbftBlockPushed(b, batch);
  }
  {
    auto b = createBlock(40, 70 * kOnePercent, 5);
    sp.pbftBlockPushed(b, batch);
  }
  {
    auto b = createBlock(50, 75 * kOnePercent, 5);
    sp.pbftBlockPushed(b, batch);
  }
  {
    auto b = createBlock(60, 70 * kOnePercent, 5);
    sp.pbftBlockPushed(b, batch);
  }
  db->commitWriteBatch(batch);

  const auto params_for_period_10_19 = cfg.vrf.threshold_upper - 5;
  const auto params_for_period_20_39 = cfg.vrf.threshold_upper - 10;
  const auto params_for_period_40_59 = cfg.vrf.threshold_upper - 1260;
  const auto params_for_period_60_and_more = cfg.vrf.threshold_upper - 3185;

  EXPECT_EQ(sp.getSortitionParams(11).vrf.threshold_upper, params_for_period_10_19);
  EXPECT_EQ(sp.getSortitionParams(19).vrf.threshold_upper, params_for_period_10_19);
  EXPECT_EQ(sp.getSortitionParams(21).vrf.threshold_upper, params_for_period_20_39);
  EXPECT_EQ(sp.getSortitionParams(29).vrf.threshold_upper, params_for_period_20_39);
  EXPECT_EQ(sp.getSortitionParams(32).vrf.threshold_upper, params_for_period_20_39);
  EXPECT_EQ(sp.getSortitionParams(39).vrf.threshold_upper, params_for_period_20_39);
  EXPECT_EQ(sp.getSortitionParams(41).vrf.threshold_upper, params_for_period_40_59);
  EXPECT_EQ(sp.getSortitionParams(51).vrf.threshold_upper, params_for_period_40_59);
  EXPECT_EQ(sp.getSortitionParams(59).vrf.threshold_upper, params_for_period_40_59);
  EXPECT_EQ(sp.getSortitionParams(69).vrf.threshold_upper, params_for_period_60_and_more);
  EXPECT_EQ(sp.getSortitionParams(79).vrf.threshold_upper, params_for_period_60_and_more);
  EXPECT_EQ(sp.getSortitionParams(210).vrf.threshold_upper, params_for_period_60_and_more);

  EXPECT_EQ(db->getParamsChangeForPeriod(11)->vrf_params.threshold_upper, params_for_period_10_19);
  EXPECT_EQ(db->getParamsChangeForPeriod(19)->vrf_params.threshold_upper, params_for_period_10_19);
  EXPECT_EQ(db->getParamsChangeForPeriod(21)->vrf_params.threshold_upper, params_for_period_20_39);
  EXPECT_EQ(db->getParamsChangeForPeriod(29)->vrf_params.threshold_upper, params_for_period_20_39);
  EXPECT_EQ(db->getParamsChangeForPeriod(32)->vrf_params.threshold_upper, params_for_period_20_39);
  EXPECT_EQ(db->getParamsChangeForPeriod(39)->vrf_params.threshold_upper, params_for_period_20_39);
  EXPECT_EQ(db->getParamsChangeForPeriod(41)->vrf_params.threshold_upper, params_for_period_40_59);
  EXPECT_EQ(db->getParamsChangeForPeriod(51)->vrf_params.threshold_upper, params_for_period_40_59);
  EXPECT_EQ(db->getParamsChangeForPeriod(59)->vrf_params.threshold_upper, params_for_period_40_59);
  EXPECT_EQ(db->getParamsChangeForPeriod(69)->vrf_params.threshold_upper, params_for_period_60_and_more);
  EXPECT_EQ(db->getParamsChangeForPeriod(79)->vrf_params.threshold_upper, params_for_period_60_and_more);
  EXPECT_EQ(db->getParamsChangeForPeriod(210)->vrf_params.threshold_upper, params_for_period_60_and_more);

  batch = db->createWriteBatch();
  {
    auto b = createBlock(70, 70 * kOnePercent, 5);
    sp.pbftBlockPushed(b, batch);
  }
  {
    auto b = createBlock(80, 80 * kOnePercent, 5);
    sp.pbftBlockPushed(b, batch);
  }
  {
    auto b = createBlock(90, 90 * kOnePercent, 5);
    sp.pbftBlockPushed(b, batch);
  }
  {
    auto b = createBlock(100, 100 * kOnePercent, 5);
    sp.pbftBlockPushed(b, batch);
  }
  db->commitWriteBatch(batch);

  // It will be already cleaned up in memory, but availiable in DB
  EXPECT_EQ(sp.getSortitionParams(11).vrf.threshold_upper, params_for_period_10_19);

  EXPECT_EQ(db->getParamsChangeForPeriod(10)->period, 10);
  EXPECT_EQ(db->getParamsChangeForPeriod(11)->period, 10);
  EXPECT_EQ(db->getParamsChangeForPeriod(19)->period, 10);
  EXPECT_EQ(db->getParamsChangeForPeriod(25)->period, 20);

  // should found data for previous interval because current was in target
  EXPECT_EQ(db->getParamsChangeForPeriod(32)->period, 20);
  // should found data for previous interval because current was in target
  EXPECT_EQ(db->getParamsChangeForPeriod(55)->period, 40);

  EXPECT_EQ(db->getParamsChangeForPeriod(72)->period, 70);
  EXPECT_EQ(db->getParamsChangeForPeriod(72)->interval_efficiency, 70 * kOnePercent);
  EXPECT_EQ(db->getParamsChangeForPeriod(80)->period, 80);
  EXPECT_EQ(db->getParamsChangeForPeriod(80)->interval_efficiency, 80 * kOnePercent);
  EXPECT_EQ(db->getParamsChangeForPeriod(95)->period, 90);
  EXPECT_EQ(db->getParamsChangeForPeriod(95)->interval_efficiency, 90 * kOnePercent);
}

}  // namespace taraxa::core_tests