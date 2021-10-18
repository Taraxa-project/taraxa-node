#include "dag/sortition_params_manager.hpp"

#include "pbft/pbft_block.hpp"
namespace taraxa {

SortitionParamsChange::SortitionParamsChange(uint16_t efficiency, VrfParams const& vrf,
                                             SortitionParamsChange const& previous)
    : vrf_params_(vrf), interval_efficiency_(efficiency) {
  actual_correction_per_percent_ = correctionPerPercent(previous);
}

SortitionParamsChange::SortitionParamsChange(uint16_t efficiency, VrfParams const& vrf)
    : vrf_params_(vrf), interval_efficiency_(efficiency) {}

uint16_t SortitionParamsChange::correctionPerPercent(SortitionParamsChange const& previous) const {
  // make calculations on vrf upper threshold
  const auto param_change = vrf_params_.threshold_upper - previous.vrf_params_.threshold_upper;
  int32_t actual_efficiency_change = interval_efficiency_ - previous.interval_efficiency_;
  // to avoid division by zero
  if (actual_efficiency_change == 0) {
    actual_efficiency_change = 1;
  }

  auto correction_per_percent = std::abs(param_change * ONE_PERCENT / actual_efficiency_change);
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
  s.appendList(4);
  s << vrf_params_.threshold_lower;
  s << vrf_params_.threshold_upper;
  s << interval_efficiency_;
  s << actual_correction_per_percent_;

  return s.out();
}

SortitionParamsChange SortitionParamsChange::from_rlp(dev::RLP const& rlp) {
  SortitionParamsChange p;

  p.vrf_params_.threshold_lower = rlp[0].toInt<uint16_t>();
  p.vrf_params_.threshold_upper = rlp[1].toInt<uint16_t>();
  p.interval_efficiency_ = rlp[2].toInt<uint16_t>();
  p.actual_correction_per_percent_ = rlp[3].toInt<uint16_t>();

  return p;
}

SortitionParamsManager::SortitionParamsManager(SortitionConfig sort_conf, std::shared_ptr<DbStorage> db)
    : config_(sort_conf), db_(db) {
  // load cache values from db
  params_changes_ = db_->getLastSortitionParams(config_.changes_count_for_average);
  // restore VRF params from last change
  if (!params_changes_.empty()) {
    config_.vrf = params_changes_.back().vrf_params_;
  }

  dag_efficiencies_.reserve(config_.computation_interval);
  dag_efficiencies_ = db_->getLastIntervalEfficiencies(config_.computation_interval);
}

uint16_t SortitionParamsManager::calculateDagEfficiency(const SyncBlock& block) const {
  size_t total_count = std::accumulate(block.dag_blocks.begin(), block.dag_blocks.end(), 0,
                                       [](uint32_t s, const auto& b) { return s + b.getTrxs().size(); });

  return block.transactions.size() * 100 * ONE_PERCENT / total_count;
}

uint16_t SortitionParamsManager::averageDagEfficiency() {
  return std::accumulate(dag_efficiencies_.begin(), dag_efficiencies_.end(), 0) / dag_efficiencies_.size();
}

uint16_t SortitionParamsManager::averageCorrectionPerPercent() const {
  if (params_changes_.empty()) return 1;
  auto sum = std::accumulate(params_changes_.begin(), params_changes_.end(), 0,
                             [](uint32_t s, const auto& e) { return s + e.actual_correction_per_percent_; });
  return sum / params_changes_.size();
}

void SortitionParamsManager::pbftBlockPushed(const SyncBlock& block) {
  uint16_t dag_efficiency = calculateDagEfficiency(block);
  dag_efficiencies_.push_back(dag_efficiency);
  db_->savePbftBlockDagEfficiency(block.pbft_blk->getPeriod(), dag_efficiency);
  LOG(log_dg_) << block.pbft_blk->getPeriod() << " pbftBlockPushed, efficiency: " << dag_efficiency / 100. << "%";

  const auto& height = block.pbft_blk->getPeriod();
  if (height % config_.computation_interval == 0) {
    recalculate(height);
  }
}

int32_t SortitionParamsManager::getChange(const uint64_t period, const uint16_t efficiency) const {
  const uint16_t per_percent = averageCorrectionPerPercent();
  int32_t correction = -(config_.target_dag_efficiency - efficiency);
  if (std::abs(correction) > config_.max_interval_correction) {
    correction = (correction < 0 ? -1 : 1) * config_.max_interval_correction;
  }
  const int32_t change = correction * per_percent / ONE_PERCENT;
  
  LOG(log_dg_) << "Average interval efficiency: " << efficiency / 100. << "%, correction per percent: " << per_percent;
  LOG(log_nf_) << "Changing VRF params on " << period << " period from (" << config_.vrf.threshold_lower << ", "
               << config_.vrf.threshold_upper << ") by " << change;

  return change;
}

void SortitionParamsManager::recalculate(const uint64_t period) {
  const auto average_dag_efficiency = averageDagEfficiency();
  dag_efficiencies_.clear();

  const int32_t change = getChange(period, average_dag_efficiency);
  config_.vrf += change;

  SortitionParamsChange params_change;
  if (params_changes_.empty()) {
    params_change = SortitionParamsChange{average_dag_efficiency, config_.vrf};
  } else {
    params_change = SortitionParamsChange{average_dag_efficiency, config_.vrf, *params_changes_.rbegin()};
  }

  db_->saveSortitionParamsChange(period, params_change);
  params_changes_.push_back(params_change);

  while (params_changes_.size() > config_.changes_count_for_average) {
    params_changes_.pop_front();
  }
}

}  // namespace taraxa