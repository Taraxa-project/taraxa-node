#pragma once

#include <fmt/ostream.h>
#include <fmt/ranges.h>
#include <libp2p/Common.h>
#include <libp2p/ENR.h>
#include <spdlog/async.h>
#include <spdlog/spdlog.h>

#include <iostream>
#include <string>

#include "common/types.hpp"
#include "config/logging_config.hpp"

namespace taraxa::logger {

// TODO: maybe use directly std::shared_ptr<spdlog::logger>
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
   */
  void Init(const LoggingConfig& logging_config);

  /**
   * @brief Deinit logger
   */
  void Deinit();

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
};

LoggingConfig CreateDefaultLoggingConfig();

}  // namespace taraxa::logger

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

template <typename T>
struct BoostOstreamFormatter : fmt::formatter<std::string> {
  template <typename FormatContext>
  auto format(const T& val, FormatContext& ctx) const {
    std::string str;
    boost::log::formatting_ostream os(str);
    os << val;
    return fmt::formatter<std::string>::format(str, ctx);
  }
};

// Specialize fmt::formatter for PeerSessionInfo
template <>
struct fmt::formatter<dev::p2p::PeerSessionInfo> : BoostOstreamFormatter<dev::p2p::PeerSessionInfo> {};

// Specialize fmt::formatter for Node
template <>
struct fmt::formatter<dev::p2p::Node> : BoostOstreamFormatter<dev::p2p::Node> {};

// Specialize fmt::formatter for bi::tcp::endpoint
template <>
struct fmt::formatter<bi::tcp::endpoint> : BoostOstreamFormatter<bi::tcp::endpoint> {};

// Specialize fmt::formatter for bi::tcp::endpoint
template <>
struct fmt::formatter<bi::udp::endpoint> : BoostOstreamFormatter<bi::udp::endpoint> {};

// Specialize fmt::formatter for dev::p2p::NodeIPEndpoint
template <>
struct fmt::formatter<dev::p2p::NodeIPEndpoint> : fmt::ostream_formatter {};

// Specialize fmt::formatter for dev::p2p::NodeID
template <>
struct fmt::formatter<dev::p2p::NodeID> : fmt::ostream_formatter {};

// Specialize fmt::formatter for ENR
template <>
struct fmt::formatter<dev::p2p::ENR> : fmt::ostream_formatter {};
