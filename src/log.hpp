#ifndef TARAXA_NODE_LOG_HPP
#define TARAXA_NODE_LOG_HPP

#include <string>

#include "chain_config.hpp"
#include "dag_block.hpp"
#include "types.hpp"
#include "util.hpp"

namespace taraxa {

class LoggingConfig;
using Logger = boost::log::sources::severity_channel_logger<>;

Verbosity stringToVerbosity(std::string _verbosity);
Logger createTaraxaLogger(int _severity, std::string const &_channel, addr_t node_id);
std::function<void()> setupLoggingConfiguration(addr_t const &node, LoggingConfig const &logging);
Logger createTaraxaLogger(int _severity, std::string const &_channel, addr_t node_id);
}  // namespace taraxa
#endif
