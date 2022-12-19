#pragma once

#include "metrics/metrics_group.hpp"

namespace metrics {
class PbftMetrics : public MetricsGroup {
 public:
  inline static const std::string group_name = "pbft";
  PbftMetrics(std::shared_ptr<prometheus::Registry> registry) : MetricsGroup(std::move(registry)) {}
  ADD_GAUGE_METRIC_WITH_UPDATER(setPeriod, "period", "Current PBFT period")
  ADD_GAUGE_METRIC_WITH_UPDATER(setRound, "round", "Current PBFT round")
  ADD_GAUGE_METRIC_WITH_UPDATER(setStep, "step", "Current PBFT step")
};
}  // namespace metrics