#pragma once

#include <boost/log/sources/severity_channel_logger.hpp>
#include <string>

#include "common/types.hpp"
#include "logger/logger_config.hpp"

namespace taraxa::logger {

// Concurrent (Thread-safe) logger type
using Logger = boost::log::sources::severity_channel_logger_mt<>;

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
 * @param verboseLevel
 * @param _channel
 * @param node_id
 * @return Logger object
 */
Logger createLogger(Verbosity verboseLevel, const std::string& channel, const addr_t& node_id);

/**
 * @brief Creates default logging config
 *
 * @return logger::Config
 */
Config createDefaultLoggingConfig();

/**
 * @brief Initializes logging according to the provided logging_config
 *
 * @param logging_config
 * @param node_id
 */
void InitLogging(Config& logging_config, const addr_t& node_id);

}  // namespace taraxa::logger

#define LOG BOOST_LOG

#define LOG_OBJECTS_DEFINE                \
  mutable taraxa::logger::Logger log_si_; \
  mutable taraxa::logger::Logger log_er_; \
  mutable taraxa::logger::Logger log_wr_; \
  mutable taraxa::logger::Logger log_nf_; \
  mutable taraxa::logger::Logger log_dg_; \
  mutable taraxa::logger::Logger log_tr_;

#define LOG_OBJECTS_REF_DEFINE     \
  taraxa::logger::Logger& log_si_; \
  taraxa::logger::Logger& log_er_; \
  taraxa::logger::Logger& log_wr_; \
  taraxa::logger::Logger& log_nf_; \
  taraxa::logger::Logger& log_dg_; \
  taraxa::logger::Logger& log_tr_;

#define LOG_OBJECTS_DEFINE_SUB(group)               \
  mutable taraxa::logger::Logger log_si_##group##_; \
  mutable taraxa::logger::Logger log_er_##group##_; \
  mutable taraxa::logger::Logger log_wr_##group##_; \
  mutable taraxa::logger::Logger log_nf_##group##_; \
  mutable taraxa::logger::Logger log_dg_##group##_; \
  mutable taraxa::logger::Logger log_tr_##group##_;

#define LOG_OBJECTS_CREATE(channel)                                                               \
  log_si_ = taraxa::logger::createLogger(taraxa::logger::Verbosity::Silent, channel, node_addr);  \
  log_er_ = taraxa::logger::createLogger(taraxa::logger::Verbosity::Error, channel, node_addr);   \
  log_wr_ = taraxa::logger::createLogger(taraxa::logger::Verbosity::Warning, channel, node_addr); \
  log_nf_ = taraxa::logger::createLogger(taraxa::logger::Verbosity::Info, channel, node_addr);    \
  log_tr_ = taraxa::logger::createLogger(taraxa::logger::Verbosity::Trace, channel, node_addr);   \
  log_dg_ = taraxa::logger::createLogger(taraxa::logger::Verbosity::Debug, channel, node_addr);

#define LOG_OBJECTS_CREATE_SUB(channel, group)                                                              \
  log_si_##group##_ = taraxa::logger::createLogger(taraxa::logger::Verbosity::Silent, channel, node_addr);  \
  log_er_##group##_ = taraxa::logger::createLogger(taraxa::logger::Verbosity::Error, channel, node_addr);   \
  log_wr_##group##_ = taraxa::logger::createLogger(taraxa::logger::Verbosity::Warning, channel, node_addr); \
  log_nf_##group##_ = taraxa::logger::createLogger(taraxa::logger::Verbosity::Info, channel, node_addr);    \
  log_tr_##group##_ = taraxa::logger::createLogger(taraxa::logger::Verbosity::Trace, channel, node_addr);   \
  log_dg_##group##_ = taraxa::logger::createLogger(taraxa::logger::Verbosity::Debug, channel, node_addr);

#define LOG_OBJECTS_INITIALIZE_FROM_SHARED(shared)                                                    \
  log_si_(shared.log_si_), log_er_(shared.log_er_), log_wr_(shared.log_wr_), log_nf_(shared.log_nf_), \
      log_dg_(shared.log_dg_), log_tr_(shared.log_tr_)