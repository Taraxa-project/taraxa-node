#pragma once

#include <prometheus/gauge.h>
#include <prometheus/registry.h>

#include <iostream>

namespace metrics {

/**
 * @brief add updater method.
 * This is used to store lambda function that updates metric, so we can update it periodically
 */
#define ADD_UPDATER_METHOD(method)                               \
  void method##Updater(MetricGetter getter) {                    \
    updaters_.push_back([this, getter]() { method(getter()); }); \
  }

/**
 * @brief add method that is setting specific gauge metric.
 */
#define ADD_GAUGE_METRIC(method, name, description)                                                  \
  void method(double v) {                                                                            \
    static auto& label = addMetric<prometheus::Gauge>(group_name + "_" + name, description).Add({}); \
    label.Set(v);                                                                                    \
  }

/**
 * @brief combines ADD_UPDATER_METHOD and ADD_GAUGE_METRIC
 */
#define ADD_GAUGE_METRIC_WITH_UPDATER(method, name, description) \
  ADD_GAUGE_METRIC(method, name, description)                    \
  ADD_UPDATER_METHOD(method)

using MetricGetter = std::function<double()>;
using MetricUpdater = std::function<void()>;
class MetricsGroup {
 public:
  MetricsGroup(std::shared_ptr<prometheus::Registry> registry) : registry_(std::move(registry)) {}
  virtual ~MetricsGroup() = default;

  /**
   * @brief template method to add metric family.
   * Family is a metric, but additional labels could be specified for it. Labels if optional.
   * @tparam Type type of prometheus metric that should be added
   * @param name metric name
   * @param help help string for metric(description)
   * @return Family<T>
   */
  template <class Type>
  prometheus::Family<Type>& addMetric(const std::string& name, const std::string& help) {
    return prometheus::detail::Builder<Type>().Name(name).Help(help).Register(*registry_);
  }
  /**
   * @brief method that is used to call registered updaters for the specific class
   */
  void updateData() {
    for (auto& update : updaters_) {
      update();
    }
  }

 protected:
  std::shared_ptr<prometheus::Registry> registry_;
  std::vector<MetricUpdater> updaters_;
};

using SharedMetricsGroup = std::shared_ptr<MetricsGroup>;
}  // namespace metrics