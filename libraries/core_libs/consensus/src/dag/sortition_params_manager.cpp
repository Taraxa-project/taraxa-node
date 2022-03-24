#include "dag/sortition_params_manager.hpp"

#include "pbft/pbft_block.hpp"

namespace taraxa {

SortitionParamsChange::SortitionParamsChange(uint64_t period, uint16_t efficiency, const VrfParams& vrf)
    : period(period), vrf_params(vrf), interval_efficiency(efficiency) {}

bytes SortitionParamsChange::rlp() const {
  dev::RLPStream s;
  s.appendList(4);
  s << vrf_params.threshold_range;
  s << vrf_params.threshold_upper;
  s << period;
  s << interval_efficiency;

  return s.invalidate();
}

SortitionParamsChange SortitionParamsChange::from_rlp(const dev::RLP& rlp) {
  SortitionParamsChange p;

  p.vrf_params.threshold_range = rlp[0].toInt<uint16_t>();
  p.vrf_params.threshold_upper = rlp[1].toInt<uint16_t>();
  p.period = rlp[2].toInt<uint64_t>();
  p.interval_efficiency = rlp[3].toInt<uint16_t>();

  return p;
}

SortitionParamsManager::SortitionParamsManager(const addr_t& node_addr, SortitionConfig sort_conf,
                                               std::shared_ptr<DbStorage> db)
    : config_(std::move(sort_conf)), db_(std::move(db)) {
  LOG_OBJECTS_CREATE("SORT_MGR");
  // load cache values from db
  params_changes_ = db_->getLastSortitionParams(config_.changes_count_for_average);
  if (params_changes_.empty()) {
    // if no changes in db save default vrf params
    auto batch = db_->createWriteBatch();
    SortitionParamsChange pc{0, config_.targetEfficiency(), config_.vrf};
    db_->saveSortitionParamsChange(0, pc, batch);
    db_->commitWriteBatch(batch);
    params_changes_.push_back(pc);
  } else {
    // restore VRF params from last change
    config_.vrf = params_changes_.back().vrf_params;
  }

  auto period = params_changes_.back().period + 1;
  ignored_efficiency_counter_ = 0;
  while (true) {
    auto data = db_->getPeriodDataRaw(period);
    if (data.size() == 0) break;
    period++;
    SyncBlock sync_block(data);
    if (sync_block.pbft_blk->getPivotDagBlockHash() != kNullBlockHash) {
      if (static_cast<int32_t>(ignored_efficiency_counter_) >=
          config_.changing_interval - config_.computation_interval) {
        dag_efficiencies_.push_back(calculateDagEfficiency(sync_block));
      } else {
        ignored_efficiency_counter_++;
      }
    }
  }
}

SortitionParams SortitionParamsManager::getSortitionParams(std::optional<uint64_t> period) const {
  if (!period || (config_.changing_interval == 0)) {
    return config_;
  }
  SortitionParams p = config_;
  auto change = db_->getParamsChangeForPeriod(period.value());
  if (change.has_value()) {
    p.vrf = change->vrf_params;
  }

  return p;
}

uint16_t SortitionParamsManager::calculateDagEfficiency(const SyncBlock& block) const {
  size_t total_transactions_count = 0;
  for (const auto& dag_block : block.dag_blocks) {
    const auto& trxs = dag_block.getTrxs();
    total_transactions_count += trxs.size();
  }

  if (total_transactions_count == 0) return 100 * kOnePercent;

  return block.transactions.size() * 100 * kOnePercent / total_transactions_count;
}

uint16_t SortitionParamsManager::averageDagEfficiency() {
  assert(dag_efficiencies_.size() > 0);
  return std::accumulate(dag_efficiencies_.begin(), dag_efficiencies_.end(), 0) / dag_efficiencies_.size();
}

void SortitionParamsManager::cleanup() {
  dag_efficiencies_.clear();
  while (params_changes_.size() > config_.changes_count_for_average) {
    params_changes_.pop_front();
  }
}

void SortitionParamsManager::pbftBlockPushed(const SyncBlock& block, DbStorage::Batch& batch,
                                             size_t non_empty_pbft_chain_size) {
  if (config_.changing_interval == 0) {
    return;
  }
  if (static_cast<int32_t>(ignored_efficiency_counter_) >= config_.changing_interval - config_.computation_interval) {
    const auto dag_efficiency = calculateDagEfficiency(block);
    dag_efficiencies_.push_back(dag_efficiency);
    const auto period = block.pbft_blk->getPeriod();
    LOG(log_dg_) << period << " pbftBlockPushed, efficiency: " << dag_efficiency / 100. << "%";

    if (non_empty_pbft_chain_size % config_.changing_interval == 0) {
      const auto params_change = calculateChange(period);
      params_changes_.push_back(params_change);
      db_->saveSortitionParamsChange(period, std::move(params_change), batch);
      cleanup();
      ignored_efficiency_counter_ = 0;
    }
  } else {
    ignored_efficiency_counter_++;
  }
}

int32_t getThresholdChange(uint16_t efficiency, uint16_t target_efficiency) {
  uint16_t deviation = std::abs(efficiency - target_efficiency) * 100 / target_efficiency;
  // If goal is 50 % return 1% change for 40%-60%, 2% change for 30%-70% and 5% change for over that
  if (deviation < 20) {
    return UINT16_MAX / 100;
  }
  if (deviation < 40) {
    return UINT16_MAX / 50;
  }
  return UINT16_MAX / 20;
}

int32_t getClosestThreshold(const EfficienciesMap& efficiencies, uint16_t target, bool is_over_target) {
  // Find the nearest greater or equal element to the target efficiency
  auto closest = efficiencies.lower_bound(target);

  // If efficiency is over target we should return nearest smaller element
  if (is_over_target) {
    --closest;
  }

  return closest->second;
}

EfficienciesMap SortitionParamsManager::getEfficienciesToUpperRange(uint16_t efficiency,
                                                                    int32_t last_threshold_upper) const {
  // efficiencies_to_uppper_range provide mapping from efficiency to VRF upper threshold, params_changes contain
  // efficiency for previous setting so mapping is done efficiency of i relates to VRF upper threshold of (i + 1)
  EfficienciesMap efficiencies_to_uppper_range;
  for (uint32_t i = 1; i < params_changes_.size(); i++) {
    efficiencies_to_uppper_range[params_changes_[i].interval_efficiency] =
        params_changes_[i - 1].vrf_params.threshold_upper;
  }
  if (params_changes_.size() > 1) {
    efficiencies_to_uppper_range[efficiency] = last_threshold_upper;
  }

  return efficiencies_to_uppper_range;
}

int32_t SortitionParamsManager::getNewUpperRange(uint16_t efficiency) const {
  assert(params_changes_.size() > 0);

  const int32_t last_threshold_upper = params_changes_.back().vrf_params.threshold_upper;
  if (efficiency >= config_.dag_efficiency_targets.first && efficiency <= config_.dag_efficiency_targets.second) {
    return last_threshold_upper;
  }

  const auto target_efficiency = config_.targetEfficiency();
  int32_t threshold_change = getThresholdChange(efficiency, target_efficiency);
  const bool is_over_target_efficiency = efficiency >= target_efficiency;
  // If we are below target the value we are changing threshold by should be negative
  if (!is_over_target_efficiency) {
    threshold_change *= -1;
  }

  auto efficiencies_to_uppper_range = getEfficienciesToUpperRange(efficiency, last_threshold_upper);

  // Check if all params are below, over target efficiency or empty. If so target is still not reached and change it by
  // calculated amount
  if (efficiencies_to_uppper_range.empty() || (efficiencies_to_uppper_range.rbegin()->first < target_efficiency) ||
      (efficiencies_to_uppper_range.begin()->first >= target_efficiency)) {
    return last_threshold_upper + threshold_change;
  }

  const auto closest_threshold =
      getClosestThreshold(efficiencies_to_uppper_range, target_efficiency, is_over_target_efficiency);

  const bool is_over_last_threshold = closest_threshold >= last_threshold_upper;

  // If current efficiency is bigger then target_efficiency and found threshold is bigger then the last_threshold
  if (is_over_target_efficiency == is_over_last_threshold) {
    // Return average between last range and closest to target_efficiency
    return (closest_threshold + last_threshold_upper) / 2;
  } else {
    return last_threshold_upper + threshold_change;
  }

  // It should not be possible to reach here
  assert(false);
}

SortitionParamsChange SortitionParamsManager::calculateChange(uint64_t period) {
  const auto average_dag_efficiency = averageDagEfficiency();

  int32_t new_upper_range = getNewUpperRange(average_dag_efficiency);
  if (new_upper_range < VrfParams::kThresholdUpperMinValue) {
    new_upper_range = VrfParams::kThresholdUpperMinValue;
  } else if (new_upper_range > UINT16_MAX) {
    new_upper_range = UINT16_MAX;
  }

  config_.vrf.threshold_upper = new_upper_range;
  LOG(log_si_) << "Average interval efficiency: " << average_dag_efficiency / 100. << "% . Changing VRF params on "
               << period << " period to (" << config_.vrf.threshold_upper << ")";

  return SortitionParamsChange{period, average_dag_efficiency, config_.vrf};
}

}  // namespace taraxa