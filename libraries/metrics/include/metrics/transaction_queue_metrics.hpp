#pragma once

#include "metrics/metrics_group.hpp"

namespace metrics {
class TransactionQueueMetrics : public MetricsGroup {
 public:
  inline static const std::string group_name = "transaction_queue";
  TransactionQueueMetrics(std::shared_ptr<prometheus::Registry> registry) : MetricsGroup(std::move(registry)) {}
  ADD_GAUGE_METRIC_WITH_UPDATER(setTransactionsCount, "transactions_count", "Transactions count in transactions queue")
  ADD_GAUGE_METRIC_WITH_UPDATER(setGasPrice, "gas_price", "Current gas price")
};
}  // namespace metrics