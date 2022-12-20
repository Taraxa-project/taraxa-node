#pragma once

#include "metrics/metrics_group.hpp"

namespace metrics {
class NetworkMetrics : public MetricsGroup {
 public:
  inline static const std::string group_name = "network";
  NetworkMetrics(std::shared_ptr<prometheus::Registry> registry) : MetricsGroup(std::move(registry)) {}
  ADD_GAUGE_METRIC_WITH_UPDATER(setPeersCount, "peers_count", "Count of peers that node is connected to")
  ADD_GAUGE_METRIC_WITH_UPDATER(setDiscoveredPeersCount, "discovered_peers_count", "Count of discovered peers")
  ADD_GAUGE_METRIC_WITH_UPDATER(setSyncingDuration, "syncing_duration_sec", "Time node is currently in sync state")
};
}  // namespace metrics