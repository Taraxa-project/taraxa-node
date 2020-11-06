#include "log.hpp"

#include "config.hpp"

//#include <Log.h>
#include <json/json.h>

#include <boost/algorithm/string.hpp>
#include <boost/core/null_deleter.hpp>
#include <fstream>

namespace taraxa {

using Logger = boost::log::sources::severity_channel_logger<>;

BOOST_LOG_ATTRIBUTE_KEYWORD(channel, "Channel", std::string)
BOOST_LOG_ATTRIBUTE_KEYWORD(short_node_id, "ShortNodeId", uint32_t)
BOOST_LOG_ATTRIBUTE_KEYWORD(node_id, "NodeId", std::string)
BOOST_LOG_ATTRIBUTE_KEYWORD(timestamp, "TimeStamp", boost::posix_time::ptime)
BOOST_LOG_ATTRIBUTE_KEYWORD(severity, "Severity", int)
BOOST_LOG_ATTRIBUTE_KEYWORD(severity_string, "SeverityStr", std::string)

std::string verbosityToString(int _verbosity) {
  switch (_verbosity) {
    case VerbositySilent:
      return "SILENT";
    case VerbosityError:
      return "ERROR";
    case VerbosityWarning:
      return "WARN";
    case VerbosityInfo:
      return "INFO";
    case VerbosityDebug:
      return "DEBUG";
    case VerbosityTrace:
      return "TRACE";
  }
  return {};
}

Verbosity stringToVerbosity(std::string _verbosity) {
  if (_verbosity == "SILENT") return VerbositySilent;
  if (_verbosity == "ERROR") return VerbosityError;
  if (_verbosity == "WARN") return VerbosityWarning;
  if (_verbosity == "INFO") return VerbosityInfo;
  if (_verbosity == "DEBUG") return VerbosityDebug;
  if (_verbosity == "TRACE") return VerbosityTrace;
  throw("Unknown verbosity string");
}

Logger createTaraxaLogger(int _severity, std::string const &_channel, addr_t node_id) {
  Logger logger(boost::log::keywords::severity = _severity, boost::log::keywords::channel = _channel);
  std::string severity_str = verbosityToString(_severity);
  logger.add_attribute("SeverityStr", boost::log::attributes::constant<std::string>(severity_str));
  logger.add_attribute("ShortNodeId", boost::log::attributes::constant<uint32_t>(*(uint32_t *)node_id.data()));
  logger.add_attribute("NodeId", boost::log::attributes::constant<std::string>(node_id.toString()));
  return logger;
}

std::function<void()> setupLoggingConfiguration(addr_t const &node, LoggingConfig const &l) {
  boost::log::core::get()->add_sink(boost::make_shared<log_sink<boost::log::sinks::text_ostream_backend>>());
  // If there is no output defined, we default to console output
  auto logging_p = make_shared<LoggingConfig>(l);
  auto &logging = *logging_p;
  if (logging.outputs.empty()) {
    logging.outputs.push_back(LoggingOutputConfig());
  }
  for (auto &output : logging.outputs) {
    auto filter = [logging_p, short_node_id_conf = *(uint32_t *)node.data()](boost::log::attribute_value_set const &_set) {
      auto const &logging = *logging_p;
      if (logging.channels.count(*_set[channel])) {
        if (short_node_id_conf == _set[short_node_id] || _set[short_node_id].empty()) {
          auto channel_name = _set[channel].get();
          if (_set[severity] > logging.channels.at(channel_name)) return false;
          return true;
        }
      } else {
        if (logging.channels.size() == 0) {
          if (_set[severity] > logging.verbosity) return false;
          return true;
        }
      }
      return false;
    };
    if (output.type == "console") {
      auto sink = boost::make_shared<log_sink<boost::log::sinks::text_ostream_backend>>();
      boost::shared_ptr<std::ostream> stream{&std::cout, boost::null_deleter{}};
      sink->locked_backend()->add_stream(stream);
      sink->set_filter(filter);

      sink->set_formatter(boost::log::aux::acquire_formatter(output.format));
      sink->locked_backend()->auto_flush(true);
      boost::log::core::get()->add_sink(sink);
      logging.console_sinks.push_back(sink);
    } else if (output.type == "file") {
      std::vector<std::string> v;
      boost::algorithm::split(v, output.time_based_rotation, boost::is_any_of(","));
      if (v.size() != 3) throw ConfigException("time_based_rotation not configured correctly" + output.time_based_rotation);
      auto sink = boost::log::add_file_log(
          boost::log::keywords::file_name = output.file_name, boost::log::keywords::rotation_size = output.rotation_size,
          boost::log::keywords::time_based_rotation = boost::log::sinks::file::rotation_at_time_point(stoi(v[0]), stoi(v[1]), stoi(v[2])),
          boost::log::keywords::max_size = output.max_size);
      sink->set_filter(filter);

      sink->set_formatter(boost::log::aux::acquire_formatter(output.format));
      sink->locked_backend()->auto_flush(true);
      boost::log::core::get()->add_sink(sink);
      logging.file_sinks.push_back(sink);
    }

    boost::log::add_common_attributes();
    boost::log::core::get()->add_global_attribute("SeverityStr", boost::log::attributes::make_function(&getThreadName));
  }

  boost::log::core::get()->set_exception_handler(boost::log::make_exception_handler<std::exception>(
      [](std::exception const &_ex) { std::cerr << "Exception from the logging library: " << _ex.what() << '\n'; }));
  return [logging_p] {
    auto const &logging = *logging_p;
    boost::log::core::get()->flush();
    for (auto &sink : logging.console_sinks) {
      boost::log::core::get()->remove_sink(sink);
    }
    for (auto &sink : logging.file_sinks) {
      boost::log::core::get()->remove_sink(sink);
    }
  };
}

}  // namespace taraxa