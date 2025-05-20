#include "spdlogger/logging.hpp"

#include "spdlog/async.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/sinks/stdout_color_sinks.h"

namespace taraxa::spdlogger {

void Logging::Init() {
  // 8k queue for messages, 1 worker thread.
  // Using one worker thread decouples the application's logging calls from the
  // actual I/O, reducing blocking in the main application threads. However, the throughput is limited by the single
  // worker thread's capacity
  spdlog::init_thread_pool(8192, 1);

  auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
  sinks_.push_back(std::move(console_sink));

  // TODO: create other syncs based on config
  // auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>("logs/async_log.txt", true);
  initialized_ = true;
}

std::shared_ptr<spdlog::logger> Logging::CreateChannelLogger(const std::string& channel) {
  if (!initialized_) {
    return nullptr;
  }

  if (auto existing_logger = spdlog::get(channel); existing_logger) {
    return existing_logger;  // Logger already exists
  }

  // TODO: use subset of sinks_ based on config

  auto logger = std::make_shared<spdlog::async_logger>(channel, sinks_.begin(), sinks_.end(), spdlog::thread_pool(),
                                                       spdlog::async_overflow_policy::block);
  spdlog::register_logger(logger);

  return logger;
}

////Config createDefaultLoggingConfig() { return Config(); }
////
////void InitLogging(Config& logging_config, const addr_t& node_id) { logging_config.InitLogging(node_id); }

}  // namespace taraxa::spdlogger