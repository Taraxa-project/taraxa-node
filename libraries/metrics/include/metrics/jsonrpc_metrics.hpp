#pragma once

#include "metrics/metrics_group.hpp"

namespace taraxa::metrics {
class JsonRpcMetrics : public MetricsGroup {
 public:
  inline static const std::string group_name = "jsonrpc";
  JsonRpcMetrics(std::shared_ptr<prometheus::Registry> registry) : MetricsGroup(std::move(registry)) {}
  const std::vector<double> buckets = {1000, 10000, 100000, 1000000, 10000000};

  ADD_HISTOGRAM_METRIC(setJsonRpcRequestDuration, "request_duration", "RPC request duration", buckets)

  // Extracting methods using string manipulation instead of JSON parsing for speed
  void report(const std::string &request, const std::string &ip, const std::string &connection,
              uint64_t request_duration) {
    std::vector<std::string_view> methods;
    std::string_view req_view(request);
    size_t start = 0;
    std::string_view method_key = "\"method\":";
    size_t method_pos = req_view.find(method_key, start);

    while (method_pos != std::string_view::npos) {
      start = method_pos + method_key.size();
      // Skip whitespace and quotes
      while (start < req_view.size() && (req_view[start] == ' ' || req_view[start] == '"')) {
        start++;
      }
      size_t end = req_view.find('"', start);
      if (end != std::string_view::npos) {
        methods.push_back(req_view.substr(start, end - start));
        start = end + 1;  // move past the closing quote
      } else {
        // Handle error: missing closing quote
        break;
      }
      method_pos = req_view.find(method_key, start);
    }

    // Convert string_view to std::string if needed by setJsonRpcRequestDuration
    for (const auto &method : methods) {
      setJsonRpcRequestDuration(request_duration,
                                {{"method", std::string(method)}, {"ip", ip}, {"connection", connection}});
    }
  }
};

}  // namespace taraxa::metrics