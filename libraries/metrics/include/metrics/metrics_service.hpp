#pragma once

#include <prometheus/exposer.h>
#include <prometheus/gauge.h>
#include <prometheus/registry.h>

#include <thread>

#include "metrics/metrics_group.hpp"

namespace taraxa::metrics {

/**
 * @brief class for metrics collecting. Registering specific metrics classes and creating prometheus server(exposer)
 */
class MetricsService {
 public:
  MetricsService(const std::string& host, uint16_t port, uint16_t polling_interval_ms);
  ~MetricsService();

  /**
   * @brief method to start thread that collecting data from special classes
   */
  void start();

  /**
   * @brief method to get specific metrics instance.
   * Creates and registers instance if it is called for first time for specified type
   *
   * @return ptr to specific metrics instance
   */
  template <class T>
  std::shared_ptr<T> getMetrics() {
    if (!exposer_) {
      return nullptr;
    }
    auto metrics = metrics_.find(T::group_name);
    if (metrics == metrics_.end()) {
      auto emplace_result = metrics_.emplace(T::group_name, std::make_shared<T>(registry_)).first;
      metrics = emplace_result;
    }
    return dynamic_pointer_cast<T>(metrics->second);
  }

 private:
  const uint16_t kPollingIntervalMs = 0;
  std::unique_ptr<prometheus::Exposer> exposer_;
  std::shared_ptr<prometheus::Registry> registry_;
  std::map<std::string, SharedMetricsGroup> metrics_;
  std::unique_ptr<std::thread> thread_;
};
}  // namespace taraxa::metrics