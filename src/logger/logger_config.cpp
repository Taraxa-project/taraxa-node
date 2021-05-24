#include "logger_config.hpp"

#include <boost/algorithm/string.hpp>
#include <boost/core/null_deleter.hpp>
#include <boost/log/attributes/function.hpp>
#include <boost/log/utility/exception_handler.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>

#include "config/config_exception.hpp"

namespace taraxa::logger {

BOOST_LOG_ATTRIBUTE_KEYWORD(channel, "Channel", std::string)
BOOST_LOG_ATTRIBUTE_KEYWORD(short_node_id, "ShortNodeId", uint32_t)
BOOST_LOG_ATTRIBUTE_KEYWORD(severity, "Severity", int)

Verbosity stringToVerbosity(std::string _verbosity) {
  if (_verbosity == "SILENT") return Verbosity::Silent;
  if (_verbosity == "ERROR") return Verbosity::Error;
  if (_verbosity == "WARN") return Verbosity::Warning;
  if (_verbosity == "INFO") return Verbosity::Info;
  if (_verbosity == "DEBUG") return Verbosity::Debug;
  if (_verbosity == "TRACE") return Verbosity::Trace;
  throw("Unknown verbosity string");
}

Config::Config(const Config &other)
    : name(other.name),
      verbosity(other.verbosity),
      channels(other.channels),
      outputs(other.outputs),
      console_sinks(other.console_sinks),
      file_sinks(other.file_sinks) {
  // logging_initialized_ flag is always set to false(in copies) so it is deinitialized
  // only in orig. config object destructor and not also in new copied Config
  logging_initialized_ = false;
}

Config &Config::operator=(const Config &other) {
  name = other.name;
  verbosity = other.verbosity;
  channels = other.channels;
  outputs = other.outputs;
  console_sinks = other.console_sinks;
  file_sinks = other.file_sinks;

  // logging_initialized_ flag is always set to false(in copies) so it is deinitialized
  // only in orig. config object destructor and not also in new copied Config
  logging_initialized_ = false;

  return *this;
}

Config::Config(Config &&other)
    : name(std::move(other.name)),
      verbosity(std::move(other.verbosity)),
      channels(std::move(other.channels)),
      outputs(std::move(other.outputs)),
      console_sinks(std::move(other.console_sinks)),
      file_sinks(std::move(other.file_sinks)),
      logging_initialized_(std::move(other.logging_initialized_)) {
  // logging_initialized_ flag in orig. object is always set to false(in moves) so it is not deinitialized
  // in destructor of the orig. config object
  other.logging_initialized_ = false;
}

Config &Config::operator=(Config &&other) {
  name = std::move(other.name);
  verbosity = std::move(other.verbosity);
  channels = std::move(other.channels);
  outputs = std::move(other.outputs);
  console_sinks = std::move(other.console_sinks);
  file_sinks = std::move(other.file_sinks);
  logging_initialized_ = std::move(other.logging_initialized_);

  // logging_initialized_ flag is always set to false(in copies) so it is deinitialized
  // only in orig. config object destructor and not also in new copied Config
  other.logging_initialized_ = false;

  return *this;
}

Config::~Config() {
  if (logging_initialized_) {
    DeinitLogging();
  }
}

void Config::InitLogging(addr_t const &node) {
  if (outputs.empty()) {
    outputs.push_back(Config::OutputConfig());
  }

  auto filter = [this, short_node_id_conf = *(uint32_t *)node.data()](boost::log::attribute_value_set const &_set) {
    if (channels.count(*_set[channel])) {
      if (short_node_id_conf == _set[short_node_id] || _set[short_node_id].empty()) {
        auto channel_name = _set[channel].get();
        if (_set[severity] > channels.at(channel_name)) return false;
        return true;
      }
    } else {
      if (channels.size() == 0) {
        if (_set[severity] > verbosity) return false;
        return true;
      }
    }
    return false;
  };

  for (auto &output : outputs) {
    if (output.type == "console") {
      auto sink = boost::make_shared<log_sink<boost::log::sinks::text_ostream_backend>>();
      boost::shared_ptr<std::ostream> stream{&std::cout, boost::null_deleter{}};
      sink->locked_backend()->add_stream(stream);
      sink->set_filter(filter);

      sink->set_formatter(boost::log::aux::acquire_formatter(output.format));
      sink->locked_backend()->auto_flush(true);
      boost::log::core::get()->add_sink(sink);
      console_sinks.push_back(sink);
    } else if (output.type == "file") {
      std::vector<std::string> v;
      boost::algorithm::split(v, output.time_based_rotation, boost::is_any_of(","));
      if (v.size() != 3)
        throw ConfigException("time_based_rotation not configured correctly" + output.time_based_rotation);
      auto sink = boost::log::add_file_log(
          boost::log::keywords::file_name = output.file_name,
          boost::log::keywords::rotation_size = output.rotation_size,
          boost::log::keywords::time_based_rotation =
              boost::log::sinks::file::rotation_at_time_point(stoi(v[0]), stoi(v[1]), stoi(v[2])),
          boost::log::keywords::max_size = output.max_size);
      sink->set_filter(filter);

      sink->set_formatter(boost::log::aux::acquire_formatter(output.format));
      sink->locked_backend()->auto_flush(true);

      boost::log::core::get()->add_sink(sink);
      file_sinks.push_back(sink);
    }

    boost::log::add_common_attributes();

    // TODO: this wouldn't work anyway, refactor our logging so it can differenciate between multiple threads logs
    // similar to aleth
    //    boost::log::core::get()->add_global_attribute("ThreadName",
    //                                                  boost::log::attributes::make_function(&::dev::getThreadName));
  }

  boost::log::core::get()->set_exception_handler(boost::log::make_exception_handler<std::exception>(
      [](std::exception const &_ex) { std::cerr << "Exception from the logging library: " << _ex.what() << '\n'; }));

  logging_initialized_ = true;
}

void Config::DeinitLogging() {
  boost::log::core::get()->flush();
  for (auto &sink : console_sinks) {
    boost::log::core::get()->remove_sink(sink);
  }
  for (auto &sink : file_sinks) {
    boost::log::core::get()->remove_sink(sink);
  }

  logging_initialized_ = false;
}

}  // namespace taraxa::logger