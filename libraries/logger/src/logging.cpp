#include "logger/logging.hpp"

#include <filesystem>

#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/sinks/rotating_file_sink.h"
#include "spdlog/sinks/stdout_color_sinks.h"

namespace taraxa::logger {

void Logging::Init(const LoggingConfig& logging_config, bool global_init) {
  if (initialized_) {
    return;
  }

  logging_config_ = logging_config;

  // 8k queue for messages, 1 worker thread.
  // Using one worker thread decouples the application's logging calls from the
  // actual I/O, reducing blocking in the main application threads. However, the throughput is limited by the single
  // worker thread's capacity
  logging_tp_ = std::make_shared<spdlog::details::thread_pool>(8192, 1);

  // Create sink lambda
  auto createSink = [](const LoggingConfig::SinkConfig& sink_config) -> spdlog::sink_ptr {
    spdlog::sink_ptr sink;

    switch (sink_config.type) {
      case LoggingConfig::LoggingType::Console:
        sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        break;
      case LoggingConfig::LoggingType::File:
        sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
            sink_config.file_full_path, sink_config.rotation_size, sink_config.max_files_num);
        break;
      default:
        throw std::runtime_error("Unknown sink type: " + std::to_string(static_cast<int>(sink_config.type)));
    }

    sink->set_level(sink_config.verbosity);
    sink->set_pattern(sink_config.format);
    return sink;
  };

  // Create sinks based on config
  for (const auto& output : logging_config_.outputs) {
    if (!output.on) {
      continue;
    }

    if (output.file_dir.has_value()) {
      std::filesystem::create_directories(*output.file_dir);
    }

    // Custom sink for specified channels
    if (output.channels.has_value()) {
      auto sink = createSink(output);
      for (const auto& channel : *output.channels) {
        specific_loggers_sinks_[channel].push_back(sink);
      }
    } else {
      // Generic sink for all channels
      all_loggers_sinks_.push_back(createSink(output));
    }
  }

  // Set up periodic flushing every 1 second
  spdlog::flush_every(std::chrono::seconds(1));

  initialized_ = true;
  global_initialized_ = global_init;
}

void Logging::Deinit(bool global_init) {
  // In case logging was initialized with global_init flag, Deinit only if called global_init flag too
  if (global_initialized_ && !global_init) {
    return;
  }

  if (initialized_) {
    spdlog::drop_all();
    spdlog::shutdown();

    all_loggers_sinks_.clear();
    specific_loggers_sinks_.clear();
    initialized_ = false;
  }
}

std::shared_ptr<spdlog::logger> Logging::CreateChannelLogger(const std::string& channel) {
  if (!initialized_) {
    return nullptr;
  }

  // Logger already exists
  if (auto existing_logger = spdlog::get(channel); existing_logger) {
    return existing_logger;
  }

  // Select all sinks for this logger
  auto sinks = all_loggers_sinks_;
  if (auto logger_sinks_it = specific_loggers_sinks_.find(channel); logger_sinks_it != specific_loggers_sinks_.end()) {
    sinks.insert(sinks.end(), logger_sinks_it->second.begin(), logger_sinks_it->second.end());
  }

  auto logger = std::make_shared<spdlog::async_logger>(channel, sinks.begin(), sinks.end(), logging_tp_,
                                                       spdlog::async_overflow_policy::block);

  if (auto channel_verbosity = logging_config_.channels.find(channel);
      channel_verbosity != logging_config_.channels.end()) {
    logger->set_level(channel_verbosity->second);
  }
  logger->flush_on(spdlog::level::err);  // Flush immediately on error or higher

  spdlog::register_logger(logger);

  return logger;
}

LoggingConfig CreateDefaultLoggingConfig() {
  LoggingConfig config;
  config.outputs.push_back(LoggingConfig::SinkConfig{});
  return config;
}

}  // namespace taraxa::logger