#include "spdlogger/logging.hpp"

#include "spdlog/async.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/sinks/basic_file_sink.h"

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

  // TODO: use subset of sinks_ based on config

  auto logger = std::make_shared<spdlog::async_logger>(
      channel,
      sinks_.begin(),
      sinks_.end(),
      spdlog::thread_pool(),
      spdlog::async_overflow_policy::block
  );
  spdlog::register_logger(logger);

  return logger;
}

//
// std::shared_ptr<spdlog::logger> createLogger(Verbosity verboseLevel, const std::string& channel,
//                                             const addr_t& node_id) {
//  Logger logger(boost::log::keywords::severity = verboseLevel, boost::log::keywords::channel = channel);
//  std::string severity_str = verbosityToString(verboseLevel);
//  logger.add_attribute("SeverityStr", boost::log::attributes::constant<std::string>(severity_str));
//  logger.add_attribute("ShortNodeId", boost::log::attributes::constant<std::string>(node_id.abridged()));
//  logger.add_attribute("NodeId", boost::log::attributes::constant<std::string>(node_id.toString()));
//  return logger;
//}
//
////Config createDefaultLoggingConfig() { return Config(); }
////
////void InitLogging(Config& logging_config, const addr_t& node_id) { logging_config.InitLogging(node_id); }

}  // namespace taraxa::Logging