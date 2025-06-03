#pragma once

#include <spdlog/async.h>
#include <spdlog/spdlog.h>

#include <string>

#include "logger/logging_config.hpp"

namespace taraxa::logger {

using Logger = std::shared_ptr<spdlog::logger>;

class Logging {
 public:
  static Logging& get() {
    static Logging instance;
    return instance;
  }

  /**
   * @brief Initializes logging according to the provided logging_config
   *
   * @param global_init
   * @param logging_config
   */
  void Init(const LoggingConfig& logging_config, bool global_init = false);

  /**
   * @brief Deinit logger
   *
   * @param global_init
   */
  void Deinit(bool global_init = false);

  /**
   * @brief Creates (or returns existing) channel logger
   *
   * @param channel
   * @return Logger object
   */
  Logger CreateChannelLogger(const std::string& channel);

 private:
  Logging() = default;
  ~Logging() = default;
  Logging(const Logging&) = delete;
  Logging& operator=(const Logging&) = delete;

  // Logging config
  LoggingConfig logging_config_;

  // Sinks for all loggers
  std::vector<spdlog::sink_ptr> all_loggers_sinks_;
  // Sinks only for specific loggers
  std::unordered_map<std::string /* logger channel name*/, std::vector<spdlog::sink_ptr>> specific_loggers_sinks_;

  // Logging threadpool
  // Using one worker thread decouples the application's logging calls from the
  // actual I/O, reducing blocking in the main application threads. However, the throughput is limited by the single
  // worker thread's capacity
  std::shared_ptr<spdlog::details::thread_pool> logging_tp_;

  bool initialized_{false};

  // If logging is initialized with global_init flag, deinitialize only if called with global_init flag too
  bool global_initialized_{false};
};

LoggingConfig CreateDefaultLoggingConfig();

}  // namespace taraxa::logger