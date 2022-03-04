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

  dag_efficiencies_ = db_->getLastIntervalEfficiencies(config_.changing_interval, config_.computation_interval);
}

SortitionParams SortitionParamsManager::getSortitionParams(std::optional<uint64_t> period) const {
  if (!period || (config_.changing_interval == 0)) {
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
  // calculate efficiency only for current block because it is not worth to check if transaction was finalized before
  size_t total_transactions_count = 0;
  std::unordered_set<trx_hash_t> unique_transactions;
  for (const auto& dag_block : block.dag_blocks) {
    if (dag_block.getDifficulty() == config_.vdf.difficulty_stale) {
      continue;
    }
    const auto& trxs = dag_block.getTrxs();
    unique_transactions.insert(trxs.begin(), trxs.end());
    total_transactions_count += trxs.size();
  }

  if (total_transactions_count == 0) return 100 * kOnePercent;

  return unique_transactions.size() * 100 * kOnePercent / total_transactions_count;
}

uint16_t SortitionParamsManager::averageDagEfficiency() {
  const auto excess_count = std::max(static_cast<int32_t>(dag_efficiencies_.size()) - config_.computation_interval, 0);
  dag_efficiencies_.erase(dag_efficiencies_.begin(), dag_efficiencies_.begin() + excess_count);
  return std::accumulate(dag_efficiencies_.begin(), dag_efficiencies_.end(), 0) / dag_efficiencies_.size();
}

void SortitionParamsManager::cleanup(uint64_t current_period) {
  const auto efficiencies_to_leave = std::max(config_.computation_interval - config_.changing_interval, 0);
  {
    const auto excess_count = std::max(static_cast<int32_t>(dag_efficiencies_.size()) - efficiencies_to_leave, 0);
    dag_efficiencies_.erase(dag_efficiencies_.begin(), dag_efficiencies_.begin() + excess_count);
  }
  {
    const auto excess_count =
        std::max(static_cast<int32_t>(params_changes_.size()) - config_.changes_count_for_average, 0);
    params_changes_.erase(params_changes_.begin(), params_changes_.begin() + excess_count);
  }
  if ((current_period - efficiencies_to_leave) > 0) {
    db_->cleanupDagEfficiencies(current_period - efficiencies_to_leave);
  }
}

void SortitionParamsManager::pbftBlockPushed(const SyncBlock& block, DbStorage::Batch& batch,
                                             size_t non_empty_pbft_chain_size) {
  if (config_.changing_interval == 0) {
    return;
  }

  uint16_t dag_efficiency = calculateDagEfficiency(block);
  dag_efficiencies_.push_back(dag_efficiency);
  const auto& period = block.pbft_blk->getPeriod();
  db_->savePbftBlockDagEfficiency(period, dag_efficiency, batch);
  LOG(log_dg_) << period << " pbftBlockPushed, efficiency: " << dag_efficiency / 100. << "%";

  if (non_empty_pbft_chain_size % config_.changing_interval == 0) {
    const auto params_change = calculateChange(non_empty_pbft_chain_size);
    if (params_change) {
      db_->saveSortitionParamsChange(period, *params_change, batch);
      params_changes_.push_back(*params_change);
    }

    cleanup(period);
  }
}

int32_t efficiencyToChange(uint16_t efficiency, uint16_t goal_efficiency) {
  uint16_t deviation = std::abs(efficiency - goal_efficiency) * 100 / goal_efficiency;
  // If goal is 50 % return 1% change for 40%-60%, 2% change for 30%-70% and 5% change for over that
  if (deviation < 20) {
    return UINT16_MAX / 100;
  }
  if (deviation < 40) {
    return UINT16_MAX / 50;
  }
  return UINT16_MAX / 20;
}

int32_t SortitionParamsManager::getNewUpperRange(uint16_t efficiency) const {
  std::map<uint16_t, uint32_t> efficiencies_to_uppper_range;
  const uint16_t goal_efficiency = (config_.dag_efficiency_targets.first + config_.dag_efficiency_targets.second) / 2;

  for (uint32_t i = 1; i < params_changes_.size(); i++) {
    efficiencies_to_uppper_range[params_changes_[i].interval_efficiency] =
        params_changes_[i - 1].vrf_params.threshold_upper;
  }
  if (params_changes_.size() > 1) {
    efficiencies_to_uppper_range[efficiency] = params_changes_[params_changes_.size() - 1].vrf_params.threshold_upper;
  }

  // Check if all last kSortitionParamsToPull are below 50%
  if ((efficiencies_to_uppper_range.empty() || efficiencies_to_uppper_range.rbegin()->first < goal_efficiency) &&
      efficiency < goal_efficiency) {
    // If last kSortitionParamsToPull are under 50% and we are still under 50%, decrease upper limit
    return ((int32_t)params_changes_[params_changes_.size() - 1].vrf_params.threshold_upper) -
           efficiencyToChange(efficiency, goal_efficiency);
  }

  // Check if all last kSortitionParamsToPull are over 50%
  if ((efficiencies_to_uppper_range.empty() || efficiencies_to_uppper_range.begin()->first >= goal_efficiency) &&
      efficiency >= goal_efficiency) {
    // If last kSortitionParamsToPull are over 50% and we are still over 50%, increase upper limit
    return ((int32_t)params_changes_[params_changes_.size() - 1].vrf_params.threshold_upper) +
           efficiencyToChange(efficiency, goal_efficiency);
  }

  // If efficiency is less than 50% find the efficiency over 50% closest to 50%
  if (efficiency < goal_efficiency) {
    for (const auto& eff : efficiencies_to_uppper_range) {
      if (eff.first >= goal_efficiency) {
        if (eff.second < params_changes_[params_changes_.size() - 1].vrf_params.threshold_upper) {
          // Return average between last range and the one over 50% and closest to 50%
          return (eff.second + params_changes_[params_changes_.size() - 1].vrf_params.threshold_upper) / 2;
        } else {
          return ((int32_t)params_changes_[params_changes_.size() - 1].vrf_params.threshold_upper) +
                 efficiencyToChange(efficiency, goal_efficiency);
        }
      }
    }
  }

  // If efficiency is over 50% find the efficiency below 50% closest to 50%
  if (efficiency >= goal_efficiency) {
    for (auto eff = efficiencies_to_uppper_range.rbegin(); eff != efficiencies_to_uppper_range.rend(); ++eff) {
      if (eff->first < goal_efficiency) {
        if (eff->second > params_changes_[params_changes_.size() - 1].vrf_params.threshold_upper) {
          // Return average between last range and the one over 50% and closest to 50%
          return (eff->second + params_changes_[params_changes_.size() - 1].vrf_params.threshold_upper) / 2;
        } else {
          return ((int32_t)params_changes_[params_changes_.size() - 1].vrf_params.threshold_upper) -
                 efficiencyToChange(efficiency, goal_efficiency);
        }
      }
    }
  }

  // It should not be possible to reach here
  assert(false);
}

std::optional<SortitionParamsChange> SortitionParamsManager::calculateChange(uint64_t period) {
  const auto average_dag_efficiency = averageDagEfficiency();
  if (average_dag_efficiency >= config_.dag_efficiency_targets.first &&
      average_dag_efficiency <= config_.dag_efficiency_targets.second) {
    LOG(log_dg_) << "Current efficiency(" << average_dag_efficiency / 100.
                 << "%) is between configured lower and upper bounds";
    return {};
  }

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