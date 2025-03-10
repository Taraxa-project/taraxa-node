#pragma once

#include <algorithm>
#include <atomic>
#include <boost/asio/strand.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <queue>
#include <string>
#include <thread>
#include <vector>

namespace taraxa::net {

struct RequestStats {
    struct MethodStats {
      size_t count{0};
      std::chrono::microseconds total_time{0};
      uint64_t averageTimeMs() const { return count > 0 ? (total_time.count()) / count / 1000 : 0; }
    };

    std::unordered_map<std::string, std::unordered_map<std::string, MethodStats>> requests_by_ip;      // IP -> {count, total_time}
    std::unordered_map<std::string, MethodStats> requests_by_method;  // Method -> {count, total_time}
    std::chrono::microseconds total_processing_time{0};           // Overall total time
    size_t total_requests{0};                                     // Overall request count
    std::mutex stats_mutex;                                       // For thread safety

    uint64_t averageProcessingTimeMs() const {
      return total_requests > 0 
          ? (total_processing_time.count()) / total_requests / 1000 
          : 0;
    }
  };
}