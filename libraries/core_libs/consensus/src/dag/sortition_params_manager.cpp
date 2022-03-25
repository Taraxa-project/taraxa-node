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

int32_t getThresholdChange(uint16_t efficiency, uint16_t target_efficiency, int32_t current_threshold) {
  uint16_t deviation = 0;
  // Deviation is measured in percentage of the range between target efficiency and 0% or 100% depending if the
  // efficiency is below or over target efficiency. For instance for target efficiency of 80% deviation for values over
  // 80% will be deviation 20% for efficiency of 84%, and deviation of 40% for efficiency of 88%. For values
  // below 80%, deviation 20% will match efficiency of 64% and deviation of 40% will match efficiency of 48%
  if (efficiency > target_efficiency) {
    deviation = (efficiency - target_efficiency) * 100 / (100 * kOnePercent - target_efficiency);
  } else if (efficiency < target_efficiency) {
    deviation = (target_efficiency - efficiency) * 100 / (target_efficiency);
  } else {
    return 0;
  }
  if (deviation < 20) {
    // If the deviation is less than 20% we are close to target so to minimize large movements, 1% of the current value
    // is used rather than 1% of the full range. For larger deviations full range is used so that we can adjust quicker
    // to any sudden large changes in the network
    return current_threshold / 100;
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
  int32_t threshold_change = getThresholdChange(efficiency, target_efficiency, last_threshold_upper);
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