#pragma once

#include "metrics/metrics_group.hpp"

namespace taraxa::metrics {
class JsonRpcMetrics : public MetricsGroup {
 public:
  inline static const std::string group_name = "jsonrpc";
  JsonRpcMetrics(std::shared_ptr<prometheus::Registry> registry) : MetricsGroup(std::move(registry)) {}
  const std::vector<double> buckets = {1000, 10000, 100000, 1000000, 10000000};
    
  ADD_HISTOGRAM_METRIC(setJsonRpcRequestDuration, "request_duration", "RPC request duration", buckets)

  //Extracting methods using string manipulation instead of JSON parsing for speed
  void report(const std::string &request, const std::string &ip, const std::string &connection, uint64_t request_duration) {
    std::vector<std::string> methods;
    size_t start = 0;
    std::string method_key = "\"method\":";
    size_t method_pos = request.find(method_key, start);
    while(method_pos != std::string::npos) {   
      start = method_pos + method_key.length();
      while(start < request.size() && (request[start] == ' ' || request[start] == '"')) {
        start++;
      }
      size_t end = request.find('"', start);
      if (end != std::string::npos) {
        methods.push_back(request.substr(start, end - start));
      }
      method_pos = request.find(method_key, start);
    }
    for(const auto &method : methods) {
      setJsonRpcRequestDuration(request_duration, {{"method", method}, {"ip", ip}, {"connection", connection}});
    }
  }
};

}  // namespace taraxa::metrics