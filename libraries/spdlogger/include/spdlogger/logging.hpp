#pragma once

#include <fmt/ranges.h>
#include <spdlog/spdlog.h>

#include <iostream>
#include <string>

#include "common/types.hpp"
#include "spdlogger/logging_config.hpp"

namespace taraxa::spdlogger {

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
   * @param logging_config
   * @param node_id
   */
  void Init(/* channels config, const addr_t& node_id*/);

  ///**
  // * @brief Creates default logging config
  // *
  // * @return logger::Config
  // */
  // Config createDefaultLoggingConfig();

  /**
   * @brief Creates thread-safe severity channel logger
   * @note To control logging in terms of where log messages are forwarded(console/file), severity filter etc..., see
   *       functions createDefaultLoggingConfig and InitLogging. Usage example:
   *
   *       // Creates logging config
   *       auto logging_config = createDefaultLoggingConfig();
   *       logging_config.verbosity = logger::Verbosity::Error;
   *       logging_config.channels["SAMPLE_CHANNEL"] = logger::Verbosity::Error;
   *
   *       // Initializes logging according to the provided config
   *       InitLogging(logging_config);
   *
   *       addr_t node_addr;
   *
   *       // Creates error logger
   *       auto logger = createLogger(logger::Verbosity::Error, "SAMPLE_CHANNEL", node_addr)
   *
   *       LOG(logger) << "sample message";
   *
   * @note see macros LOG_OBJECTS_DEFINE, LOG_OBJECTS_CREATE, LOG
   * @param channel
   * @return Logger object
   */
  Logger CreateChannelLogger(const std::string& channel);

 private:
  Logging() = default;
  ~Logging() = default;
  Logging(const Logging&) = delete;
  Logging& operator=(const Logging&) = delete;

  // TODO: add channels/verbosity/sinks config

  std::vector<spdlog::sink_ptr> sinks_;
  bool initialized_{false};
};

}  // namespace taraxa::spdlogger

// Logger fmt::formatters for custom types

// Specialize fmt::formatter for std::atomic<T>
template <typename T>
struct fmt::formatter<std::atomic<T>> : fmt::formatter<T> {
  template <typename FormatContext>
  auto format(const std::atomic<T>& atomic_val, FormatContext& ctx) const {
    return fmt::formatter<T>::format(atomic_val.load(std::memory_order_relaxed), ctx);
  }
};

// Specialize fmt::formatter for taraxa::blk_hash_t
template <>
struct fmt::formatter<taraxa::blk_hash_t> : fmt::formatter<std::string> {
  template <typename FormatContext>
  auto format(const taraxa::blk_hash_t& val, FormatContext& ctx) const {
    return fmt::format_to(ctx.out(), "{}", val.abridged());
  }
};

// Specialize fmt::formatter for taraxa::addr_t
template <>
struct fmt::formatter<taraxa::addr_t> : fmt::formatter<std::string> {
  template <typename FormatContext>
  auto format(const taraxa::addr_t& val, FormatContext& ctx) const {
    return fmt::format_to(ctx.out(), "{}", val.abridged());
  }
};
