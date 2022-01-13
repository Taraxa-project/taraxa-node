#include "dag/sortition_params_manager.hpp"

#include "pbft/pbft_block.hpp"

namespace taraxa {

constexpr uint16_t k_testnet_hardfork2_sortition_interval_length = 200;
constexpr uint16_t k_testnet_hardfork2_difficulty_stale = 23;

SortitionParamsChange::SortitionParamsChange(uint64_t period, uint16_t efficiency, const VrfParams& vrf,
                                             const SortitionParamsChange& previous)
    : period(period), vrf_params(vrf), interval_efficiency(efficiency) {
  actual_correction_per_percent = correctionPerPercent(previous);
}

SortitionParamsChange::SortitionParamsChange(uint64_t period, uint16_t efficiency, const VrfParams& vrf)
    : period(period), vrf_params(vrf), interval_efficiency(efficiency) {}

uint16_t SortitionParamsChange::correctionPerPercent(const SortitionParamsChange& previous) const {
  // make calculations on vrf upper threshold
  const auto param_change = vrf_params.threshold_upper - previous.vrf_params.threshold_upper;
  int32_t actual_efficiency_change = interval_efficiency - previous.interval_efficiency;
  // to avoid division by zero
  if (actual_efficiency_change == 0) {
    actual_efficiency_change = 1;
  }

  auto correction_per_percent = std::abs(param_change * kOnePercent / actual_efficiency_change);
  // zero correction could lead to stuck of that protocol and give up with zero corrections
  if (correction_per_percent == 0) {
    correction_per_percent = 1;
  }
  // correction per percent shouldn't be bigger than percent of value
  correction_per_percent = std::min(correction_per_percent, std::numeric_limits<uint16_t>::max() / 100);
  return correction_per_percent;
}

bytes SortitionParamsChange::rlp() const {
  dev::RLPStream s;
  s.appendList(5);
  s << vrf_params.threshold_lower;
  s << vrf_params.threshold_upper;
  s << period;
  s << interval_efficiency;
  s << actual_correction_per_percent;

  return s.out();
}

SortitionParamsChange SortitionParamsChange::from_rlp(const dev::RLP& rlp) {
  SortitionParamsChange p;

  p.vrf_params.threshold_lower = rlp[0].toInt<uint16_t>();
  p.vrf_params.threshold_upper = rlp[1].toInt<uint16_t>();
  p.period = rlp[2].toInt<uint64_t>();
  p.interval_efficiency = rlp[3].toInt<uint16_t>();
  p.actual_correction_per_percent = rlp[4].toInt<uint16_t>();

  return p;
}

void SortitionParamsManager::fixDifficulty() {
  // Testnet fix fork for incorrect sortition params changes
  auto params_change = db_->getParamsChangeForPeriod(155400);
  if (params_change.has_value() && params_change->period == 155400) {
    auto batch = db_->createWriteBatch();
    VrfParams vrf_params;
    vrf_params.threshold_lower = 0;
    vrf_params.threshold_upper = 1866;
    SortitionParamsChange period_155400(155400, 6944, vrf_params);
    period_155400.actual_correction_per_percent = 15;
    db_->saveSortitionParamsChange(155400, period_155400, batch);

    vrf_params.threshold_lower = 0;
    vrf_params.threshold_upper = 2306;
    SortitionParamsChange period_155450(155450, 6771, vrf_params);
    period_155400.actual_correction_per_percent = 254;
    db_->saveSortitionParamsChange(155450, period_155450, batch);

    vrf_params.threshold_lower = 0;
    vrf_params.threshold_upper = 3176;
    SortitionParamsChange period_155500(155500, 8390, vrf_params);
    period_155400.actual_correction_per_percent = 53;
    db_->saveSortitionParamsChange(155500, period_155500, batch);

    vrf_params.threshold_lower = 0;
    vrf_params.threshold_upper = 3966;
    SortitionParamsChange period_155550(155550, 9831, vrf_params);
    period_155400.actual_correction_per_percent = 54;
    db_->saveSortitionParamsChange(155550, period_155550, batch);

    vrf_params.threshold_lower = 0;
    vrf_params.threshold_upper = 4826;
    SortitionParamsChange period_155600(155600, 9875, vrf_params);
    period_155400.actual_correction_per_percent = 655;
    db_->saveSortitionParamsChange(155600, period_155600, batch);

    db_->commitWriteBatch(batch);
  }
  /*
    {"id":"0","jsonrpc":"2.0","result":{"actual_correction_per_percent":655,"interval_efficiency":9875,"k_threshold_range":6144,"k_threshold_upper_min_value":80,"period":155600,"threshold_lower":0,"threshold_upper":4826}}
    {"id":"0","jsonrpc":"2.0","result":{"actual_correction_per_percent":54,"interval_efficiency":9831,"k_threshold_range":6144,"k_threshold_upper_min_value":80,"period":155550,"threshold_lower":0,"threshold_upper":3966}}
    {"id":"0","jsonrpc":"2.0","result":{"actual_correction_per_percent":53,"interval_efficiency":8390,"k_threshold_range":6144,"k_threshold_upper_min_value":80,"period":155500,"threshold_lower":0,"threshold_upper":3176}}
    {"id":"0","jsonrpc":"2.0","result":{"actual_correction_per_percent":254,"interval_efficiency":6771,"k_threshold_range":6144,"k_threshold_upper_min_value":80,"period":155450,"threshold_lower":0,"threshold_upper":2306}}
    {"id":"0","jsonrpc":"2.0","result":{"actual_correction_per_percent":15,"interval_efficiency":6944,"k_threshold_range":6144,"k_threshold_upper_min_value":80,"period":155400,"threshold_lower":0,"threshold_upper":1866}}

*/
}

SortitionParamsManager::SortitionParamsManager(const addr_t& node_addr, SortitionConfig sort_conf,
                                               std::shared_ptr<DbStorage> db)
    : config_(std::move(sort_conf)), db_(std::move(db)) {
  LOG_OBJECTS_CREATE("SORT_MGR");

  fixDifficulty();

  // load cache values from db
  params_changes_ = db_->getLastSortitionParams(config_.changes_count_for_average);
  // restore VRF params from last change
  if (!params_changes_.empty()) {
    config_.vrf = params_changes_.back().vrf_params;
  }

  auto changes_count_to_calculate = config_.computation_interval;
  auto computation_interval = config_.computation_interval;
  if (!params_changes_.empty() && params_changes_.back().period >= k_testnet_hardfork2_block_num) {
    changes_count_to_calculate = k_testnet_hardfork2_sortition_interval_length;
  } else if (!params_changes_.empty() && params_changes_.back().period >= k_testnet_hardfork3_block_num) {
    computation_interval = k_testnet_hardfork2_sortition_interval_length;
  }
  dag_efficiencies_ = db_->getLastIntervalEfficiencies(computation_interval, changes_count_to_calculate);
}

uint64_t SortitionParamsManager::currentProposalPeriod() const {
  if (params_changes_.empty()) return 0;

  return params_changes_.rbegin()->period;
}

SortitionParams SortitionParamsManager::getSortitionParams(std::optional<uint64_t> period) const {
  if (!period) {
    return config_;
  }
  bool is_period_params_found = false;
  SortitionParams p = config_;
  auto computation_interval = config_.computation_interval;
  if (period >= k_testnet_hardfork3_block_num) {
    computation_interval = k_testnet_hardfork2_sortition_interval_length;
  }
  for (auto prev = params_changes_.rbegin(), it = params_changes_.rbegin(); it != params_changes_.rend(); prev = it++) {
    auto upper_bound = prev->period;
    if (it->period == upper_bound) {
      upper_bound = it->period + computation_interval;
    }
    if (period >= it->period && period < upper_bound) {
      is_period_params_found = true;
      p.vrf = it->vrf_params;
    }
  }
  if (!is_period_params_found) {
    auto change = db_->getParamsChangeForPeriod(period.value());
    if (change.has_value()) {
      p.vrf = change->vrf_params;
    }
  }

  // Testnet hotfix
  if (period >= k_testnet_hardfork2_block_num) {
    p.vdf.difficulty_stale = k_testnet_hardfork2_difficulty_stale;
  }

  return p;
}

uint16_t calculateEfficiencyHF2(const SyncBlock& block, uint16_t stale_difficulty) {
  // calculate efficiency only for current block because it is not worth to check if transaction was finalized before
  size_t total_transactions_count = 0;
  std::unordered_set<trx_hash_t> unique_transactions;
  for (const auto& dag_block : block.dag_blocks) {
    if (dag_block.getDifficulty() == stale_difficulty ||
        dag_block.getDifficulty() == k_testnet_hardfork2_difficulty_stale) {
      continue;
    }
    const auto& trxs = dag_block.getTrxs();
    unique_transactions.insert(trxs.begin(), trxs.end());
    total_transactions_count += trxs.size();
  }

  if (total_transactions_count == 0) return 100 * kOnePercent;

  return unique_transactions.size() * 100 * kOnePercent / total_transactions_count;
}

uint16_t SortitionParamsManager::calculateDagEfficiency(const SyncBlock& block) const {
  if (block.pbft_blk->getPeriod() >= k_testnet_hardfork2_block_num) {
    return calculateEfficiencyHF2(block, config_.vdf.difficulty_stale);
  }

  size_t total_count = std::accumulate(block.dag_blocks.begin(), block.dag_blocks.end(), 0,
                                       [](uint32_t s, const auto& b) { return s + b.getTrxs().size(); });

  return block.transactions.size() * 100 * kOnePercent / total_count;
}

uint16_t SortitionParamsManager::averageDagEfficiency() {
  return std::accumulate(dag_efficiencies_.begin(), dag_efficiencies_.end(), 0) / dag_efficiencies_.size();
}

uint16_t SortitionParamsManager::averageCorrectionPerPercent() const {
  if (params_changes_.empty()) return 1;
  auto sum = std::accumulate(params_changes_.begin(), params_changes_.end(), 0,
                             [](uint32_t s, const auto& e) { return s + e.actual_correction_per_percent; });
  return sum / params_changes_.size();
}

void SortitionParamsManager::cleanup(uint64_t current_period) {
  uint16_t efficiencies_to_leave = 0;
  if (current_period >= k_testnet_hardfork2_block_num && current_period < k_testnet_hardfork3_block_num) {
    efficiencies_to_leave = k_testnet_hardfork2_sortition_interval_length - config_.computation_interval;
  }

  while (params_changes_.size() > config_.changes_count_for_average) {
    params_changes_.pop_front();
  }
  while (dag_efficiencies_.size() > efficiencies_to_leave) {
    dag_efficiencies_.pop_front();
  }
  if ((current_period - efficiencies_to_leave) > 0) {
    auto batch = db_->createWriteBatch();
    db_->cleanupDagEfficiencies(current_period - efficiencies_to_leave);
    db_->commitWriteBatch(batch);
  }
}

void SortitionParamsManager::pbftBlockPushed(const SyncBlock& block, DbStorage::Batch& batch) {
  const auto& period = block.pbft_blk->getPeriod();
  if (period == 155601) {
    fixDifficulty();
    params_changes_ = db_->getLastSortitionParams(config_.changes_count_for_average);
  }
  uint16_t dag_efficiency = calculateDagEfficiency(block);
  dag_efficiencies_.push_back(dag_efficiency);
  db_->savePbftBlockDagEfficiency(period, dag_efficiency, batch);
  LOG(log_dg_) << period << " pbftBlockPushed, efficiency: " << dag_efficiency / 100. << "%";

  auto computation_interval = config_.computation_interval;
  if (period >= k_testnet_hardfork3_block_num) {
    computation_interval = k_testnet_hardfork2_sortition_interval_length;
  }
  if (period % computation_interval == 0) {
    const auto params_change = calculateChange(period);
    if (params_change) {
      db_->saveSortitionParamsChange(period, *params_change, batch);
      params_changes_.push_back(*params_change);
    }

    cleanup(period);
  }
}

int32_t SortitionParamsManager::getChange(uint64_t period, uint16_t efficiency) const {
  const uint16_t per_percent = averageCorrectionPerPercent();
  int32_t correction = -(config_.targetEfficiency() - efficiency);
  if (std::abs(correction) > config_.max_interval_correction) {
    correction = (correction < 0 ? -1 : 1) * config_.max_interval_correction;
  }
  const int32_t change = correction * per_percent / kOnePercent;

  LOG(log_dg_) << "Average interval efficiency: " << efficiency / 100. << "%, correction per percent: " << per_percent;
  LOG(log_nf_) << "Changing VRF params on " << period << " period from (" << config_.vrf.threshold_lower << ", "
               << config_.vrf.threshold_upper << ") by " << change;

  return change;
}

std::optional<SortitionParamsChange> SortitionParamsManager::calculateChange(uint64_t period) {
  const auto average_dag_efficiency = averageDagEfficiency();
  if (average_dag_efficiency >= config_.dag_efficiency_targets.first &&
      average_dag_efficiency <= config_.dag_efficiency_targets.second) {
    LOG(log_dg_) << "Current efficiency(" << average_dag_efficiency / 100.
                 << "%) is between configured lower and upper bounds";
    return {};
  }

  const int32_t change = getChange(period, average_dag_efficiency);
  config_.vrf.addChange(change, period >= k_threshold_testnet_hard_fork_period);

  if (params_changes_.empty()) {
    return SortitionParamsChange{period, average_dag_efficiency, config_.vrf};
  }

  return SortitionParamsChange{period, average_dag_efficiency, config_.vrf, *params_changes_.rbegin()};
}

}  // namespace taraxa