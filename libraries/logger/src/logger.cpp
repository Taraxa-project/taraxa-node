#include "logger/logger.hpp"

#include <boost/log/utility/setup/common_attributes.hpp>

namespace taraxa::logger {

Logger createLogger(Verbosity verboseLevel, const std::string& channel, const addr_t& node_id) {
  Logger logger(boost::log::keywords::severity = verboseLevel, boost::log::keywords::channel = channel);
  std::string severity_str = logger::verbosityToString(verboseLevel);
  logger.add_attribute("SeverityStr", boost::log::attributes::constant<std::string>(severity_str));
  logger.add_attribute("ShortNodeId", boost::log::attributes::constant<std::string>(node_id.abridged()));
  logger.add_attribute("NodeId", boost::log::attributes::constant<std::string>(node_id.toString()));
  return logger;
}

Config createDefaultLoggingConfig() { return Config(); }

void InitLogging(Config& logging_config, const addr_t& node_id) { logging_config.InitLogging(node_id); }

}  // namespace taraxa::logger