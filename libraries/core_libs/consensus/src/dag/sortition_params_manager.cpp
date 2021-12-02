#include "dag/sortition_params_manager.hpp"

#include "pbft/pbft_block.hpp"
namespace taraxa {

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

  if (p.vrf_params.threshold_lower == 0) {
    assert(p.vrf_params.threshold_upper <= p.vrf_params.k_threshold_range);
  } else {
    assert(p.vrf_params.threshold_upper - p.vrf_params.threshold_lower == p.vrf_params.k_threshold_range);
  }

  return p;
}

SortitionParamsManager::SortitionParamsManager(const addr_t& node_addr, SortitionConfig sort_conf,
                                               std::shared_ptr<DbStorage> db)
    : config_(std::move(sort_conf)), db_(std::move(db)) {
  LOG_OBJECTS_CREATE("TARCAP");
  // load cache values from db
  params_changes_ = db_->getLastSortitionParams(config_.changes_count_for_average);
  // restore VRF params from last change
  if (!params_changes_.empty()) {
    config_.vrf = params_changes_.back().vrf_params;
  }

  dag_efficiencies_.reserve(config_.computation_interval);
  dag_efficiencies_ = db_->getLastIntervalEfficiencies(config_.computation_interval);
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
  for (auto prev = params_changes_.rbegin(), it = params_changes_.rbegin(); it != params_changes_.rend(); prev = it++) {
    auto upper_bound = prev->period;
    if (it->period == upper_bound) {
      upper_bound = it->period + config_.computation_interval;
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

  return p;
}

uint16_t SortitionParamsManager::calculateDagEfficiency(const SyncBlock& block) const {
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
  dag_efficiencies_.clear();
  while (params_changes_.size() > config_.changes_count_for_average) {
    params_changes_.pop_front();
  }
  auto batch = db_->createWriteBatch();
  db_->cleanupDagEfficiencies(current_period);
  db_->commitWriteBatch(batch);
}

void SortitionParamsManager::pbftBlockPushed(const SyncBlock& block, DbStorage::Batch& batch) {
  uint16_t dag_efficiency = calculateDagEfficiency(block);
  dag_efficiencies_.push_back(dag_efficiency);
  const auto& period = block.pbft_blk->getPeriod();
  db_->savePbftBlockDagEfficiency(period, dag_efficiency, batch);
  LOG(log_dg_) << period << " pbftBlockPushed, efficiency: " << dag_efficiency / 100. << "%";

  const auto& height = period;
  if (height % config_.computation_interval == 0) {
    const auto params_change = calculateChange(height);
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