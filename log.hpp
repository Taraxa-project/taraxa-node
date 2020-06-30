#ifndef TARAXA_NODE_LOG_HPP
#define TARAXA_NODE_LOG_HPP

#include <boost/log/attributes/clock.hpp>
#include <boost/log/attributes/function.hpp>
#include <boost/log/detail/sink_init_helpers.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/sinks/sync_frontend.hpp>
#include <boost/log/sinks/text_ostream_backend.hpp>
#include <boost/log/sources/channel_feature.hpp>
#include <boost/log/sources/channel_logger.hpp>
#include <boost/log/sources/global_logger_storage.hpp>
#include <boost/log/sources/severity_channel_logger.hpp>
#include <boost/log/support/date_time.hpp>
#include <boost/log/utility/exception_handler.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/log/utility/setup/file.hpp>
#include <string>

#include "chain_config.hpp"
#include "dag_block.hpp"
#include "types.hpp"
#include "util.hpp"

namespace taraxa {
class LoggingConfig;
using Logger = boost::log::sources::severity_channel_logger<>;

int stringToVerbosity(std::string _verbosity);
Logger createTaraxaLogger(int _severity, std::string const &_channel,
                          addr_t node_id);
void setupLoggingConfiguration(addr_t &node, LoggingConfig &logging);
void removeLogging(LoggingConfig &logging);
Logger createTaraxaLogger(int _severity, std::string const &_channel,
                          addr_t node_id);
}  // namespace taraxa
#endif
