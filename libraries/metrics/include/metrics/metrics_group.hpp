#pragma once

#include <prometheus/gauge.h>
#include <prometheus/histogram.h>
#include <prometheus/registry.h>

namespace taraxa::metrics {

/**
 * @brief add method that is setting specific gauge metric.
 */
#define ADD_GAUGE_METRIC(method, name, description)                                                  \
  void method(double v) {                                                                            \
    static auto& label = addMetric<prometheus::Gauge>(group_name + "_" + name, description).Add({}); \
    label.Set(v);                                                                                    \
  }

/**
 * @brief add method that is setting specific histogram metric.
 */
#define ADD_HISTOGRAM_METRIC(method, name, description, buckets)                                           \
  void method(double v, std::map<std::string, std::string> labels) {                                       \
    static auto& label = addMetric<prometheus::Histogram>(group_name + "_" + name, description);           \
    label.Add(labels, prometheus::Histogram::BucketBoundaries{buckets.begin(), buckets.end()}).Observe(v); \
  }

/**
 * @brief add updater method.
 * This is used to store lambda function that updates metric, so we can update it periodically
 * Passed `method` should be added first
 */
#define ADD_UPDATER_METHOD(method)                               \
  void method##Updater(MetricGetter getter) {                    \
    updaters_.push_back([this, getter]() { method(getter()); }); \
  }

/**
 * @brief combines ADD_UPDATER_METHOD and ADD_GAUGE_METRIC
 */
#define ADD_GAUGE_METRIC_WITH_UPDATER(method, name, description) \
  ADD_GAUGE_METRIC(method, name, description)                    \
  ADD_UPDATER_METHOD(method)

class MetricsGroup {
 public:
  using MetricGetter = std::function<double()>;
  using MetricUpdater = std::function<void()>;
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
}  // namespace taraxa::metrics