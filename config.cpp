#include "config.hpp"

//#include <Log.h>
#include <json/json.h>

#include <boost/algorithm/string.hpp>
#include <boost/core/null_deleter.hpp>
#include <fstream>

namespace taraxa {

BOOST_LOG_ATTRIBUTE_KEYWORD(channel, "Channel", std::string)
BOOST_LOG_ATTRIBUTE_KEYWORD(node_id, "NodeId", uint32_t)
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

int stringToVerbosity(std::string _verbosity) {
  if (_verbosity == "SILENT") return VerbositySilent;
  if (_verbosity == "ERROR") return VerbosityError;
  if (_verbosity == "WARN") return VerbosityWarning;
  if (_verbosity == "INFO") return VerbosityInfo;
  if (_verbosity == "DEBUG") return VerbosityDebug;
  if (_verbosity == "TRACE") return VerbosityTrace;
  throw("Unknown verbosity string");
}

std::string getConfigErr(std::vector<string> path) {
  std::string res = "Error in processing configuration file on param: ";
  for (auto i = 0; i < path.size(); i++) res += path[i] + ".";
  res += " ";
  return res;
}

Json::Value getConfigData(Json::Value root, std::vector<string> const &path,
                          bool optional = false) {
  for (auto i = 0; i < path.size(); i++) {
    root = root[path[i]];
    if (root.isNull() && !optional) {
      throw ConfigException(getConfigErr(path) + "Element missing: " + path[i]);
    }
  }
  return root;
}

std::string getConfigDataAsString(Json::Value &root,
                                  std::vector<string> const &path) {
  try {
    return getConfigData(root, path).asString();
  } catch (Json::Exception &e) {
    throw ConfigException(getConfigErr(path) + e.what());
  }
}

uint32_t getConfigDataAsUInt(Json::Value &root, std::vector<string> const &path,
                             bool optional = false) {
  try {
    return getConfigData(root, path, optional).asUInt();
  } catch (Json::Exception &e) {
    if (optional) {
      return 0;
    }
    throw ConfigException(getConfigErr(path) + e.what());
  }
}

uint64_t getConfigDataAsUInt64(Json::Value &root,
                               std::vector<string> const &path) {
  try {
    return getConfigData(root, path).asUInt64();
  } catch (Json::Exception &e) {
    throw ConfigException(getConfigErr(path) + e.what());
  }
}

bool getConfigDataAsBoolean(Json::Value &root,
                            std::vector<string> const &path) {
  try {
    return getConfigData(root, path).asBool();
  } catch (Json::Exception &e) {
    throw ConfigException(getConfigErr(path) + e.what());
  }
}

using Logger = boost::log::sources::severity_channel_logger<>;
Logger createTaraxaLogger(int _severity, std::string const &_channel,
                          addr_t node_id) {
  Logger logger(boost::log::keywords::severity = _severity,
                boost::log::keywords::channel = _channel);
  std::string severity_str = verbosityToString(_severity);
  logger.add_attribute(
      "SeverityStr",
      boost::log::attributes::constant<std::string>(severity_str));
  logger.add_attribute("NodeId", boost::log::attributes::constant<uint32_t>(
                                     *(uint32_t *)&node_id));
  return logger;
}

void LoggingConfig::removeLogging() {
  boost::log::core::get()->flush();
  for (auto &sink : console_sinks) {
    boost::log::core::get()->remove_sink(sink);
  }
  for (auto &sink : file_sinks) {
    boost::log::core::get()->remove_sink(sink);
  }
}

void LoggingConfig::setupLoggingConfiguration(addr_t node) {
  uint32_t short_node_id = *(uint32_t *)&node;
  boost::log::core::get()->add_sink(
      boost::make_shared<log_sink<boost::log::sinks::text_ostream_backend>>());
  for (auto &output : outputs) {
    if (output.type == "console") {
      auto sink = boost::make_shared<
          log_sink<boost::log::sinks::text_ostream_backend>>();
      boost::shared_ptr<std::ostream> stream{&std::cout, boost::null_deleter{}};
      sink->locked_backend()->add_stream(stream);
      sink->set_filter(
          [this, short_node_id](boost::log::attribute_value_set const &_set) {
            if (channels.count(*_set[channel])) {
              if (short_node_id == _set[node_id]) {
                auto channel_name = _set[channel].get();
                if (_set[severity] > channels.at(channel_name)) return false;
                return true;
              }
            }
            return false;
          });

      sink->set_formatter(boost::log::aux::acquire_formatter(output.format));
      sink->locked_backend()->auto_flush(true);
      boost::log::core::get()->add_sink(sink);
      console_sinks.push_back(sink);
    } else if (output.type == "file") {
      std::vector<std::string> v;
      boost::algorithm::split(v, output.time_based_rotation,
                              boost::is_any_of(","));
      if (v.size() != 3)
        throw ConfigException("time_based_rotation not configured correctly" +
                              output.time_based_rotation);
      auto sink = boost::log::add_file_log(
          boost::log::keywords::file_name = output.file_name,
          boost::log::keywords::rotation_size = output.rotation_size,
          boost::log::keywords::time_based_rotation =
              boost::log::sinks::file::rotation_at_time_point(
                  stoi(v[0]), stoi(v[1]), stoi(v[2])),
          boost::log::keywords::max_size = output.max_size);
      sink->set_filter(
          [this, short_node_id](boost::log::attribute_value_set const &_set) {
            if (channels.count(*_set[channel])) {
              if (short_node_id == _set[node_id]) {
                auto channel_name = _set[channel].get();
                if (_set[severity] > channels.at(channel_name)) return false;
                return true;
              }
            }
            return false;
          });

      sink->set_formatter(boost::log::aux::acquire_formatter(output.format));
      sink->locked_backend()->auto_flush(true);
      boost::log::core::get()->add_sink(sink);
      file_sinks.push_back(sink);
    }

    boost::log::add_common_attributes();
    boost::log::core::get()->add_global_attribute(
        "SeverityStr", boost::log::attributes::make_function(&getThreadName));
  }

  boost::log::core::get()->set_exception_handler(
      boost::log::make_exception_handler<std::exception>(
          [](std::exception const &_ex) {
            std::cerr << "Exception from the logging library: " << _ex.what()
                      << '\n';
          }));
}

FullNodeConfig::FullNodeConfig(std::string const &json_file)
    : json_file_name(json_file) {
  Json::Value root;
  std::ifstream config_doc(json_file, std::ifstream::binary);
  if (!config_doc.is_open()) {
    throw ConfigException(string("Could not open configuration file: ") +
                          json_file);
  }
  try {
    config_doc >> root;
  } catch (Json::Exception &e) {
    throw ConfigException(string("Could not parse json configuration file: ") +
                          json_file + e.what());
  }

  node_secret = getConfigDataAsString(root, {"node_secret"});
  vrf_secret = getConfigDataAsString(root, {"vrf_secret"});
  db_path = getConfigDataAsString(root, {"db_path"});
  dag_processing_threads =
      getConfigDataAsUInt(root, {"dag_processing_threads"});

  network.network_address = getConfigDataAsString(root, {"network_address"});
  network.network_id = getConfigDataAsString(root, {"network_id"});
  network.network_listen_port =
      getConfigDataAsUInt(root, {"network_listen_port"});
  network.network_simulated_delay =
      getConfigDataAsUInt(root, {"network_simulated_delay"});
  network.network_transaction_interval =
      getConfigDataAsUInt(root, {"network_transaction_interval"});
  network.network_bandwidth = getConfigDataAsUInt(root, {"network_bandwidth"});
  network.network_ideal_peer_count =
      getConfigDataAsUInt(root, {"network_ideal_peer_count"});
  network.network_max_peer_count =
      getConfigDataAsUInt(root, {"network_max_peer_count"});
  network.network_sync_level_size =
      getConfigDataAsUInt(root, {"network_sync_level_size"});
  network.network_encrypted =
      getConfigDataAsUInt(root, {"network_encrypted"}) != 0;
  network.network_performance_log =
      getConfigDataAsUInt(root, {"network_performance_log"}) & 1;
  for (auto &item : root["network_boot_nodes"]) {
    NodeConfig node;
    node.id = getConfigDataAsString(item, {"id"});
    node.ip = getConfigDataAsString(item, {"ip"});
    node.port = getConfigDataAsUInt(item, {"port"});
    network.network_boot_nodes.push_back(node);
  }
  rpc.address = boost::asio::ip::address::from_string(network.network_address);
  rpc.port = getConfigDataAsUInt(root, {"rpc_port"});
  rpc.ws_port = getConfigDataAsUInt(root, {"ws_port"});
  {  // for test experiments
    test_params.max_transaction_queue_warn = getConfigDataAsUInt(
        root, {"test_params", "max_transaction_queue_warn"}, true);
    test_params.max_transaction_queue_drop = getConfigDataAsUInt(
        root, {"test_params", "max_transaction_queue_drop"}, true);

    test_params.max_block_queue_warn = getConfigDataAsUInt(
        root, {"test_params", "max_block_queue_warn"}, true);

    test_params.block_proposer.mode =
        getConfigDataAsString(root, {"test_params", "block_proposer", "mode"});
    test_params.block_proposer.shard =
        getConfigDataAsUInt(root, {"test_params", "block_proposer", "shard"});
    test_params.block_proposer.transaction_limit = getConfigDataAsUInt(
        root, {"test_params", "block_proposer", "transaction_limit"});
    if (test_params.block_proposer.mode == "random") {
      test_params.block_proposer.min_freq = getConfigDataAsUInt(
          root, {"test_params", "block_proposer", "random_params", "min_freq"});
      test_params.block_proposer.max_freq = getConfigDataAsUInt(
          root, {"test_params", "block_proposer", "random_params", "max_freq"});
    } else if (test_params.block_proposer.mode == "sortition") {
      test_params.block_proposer.difficulty_bound =
          getConfigDataAsUInt(root, {"test_params", "block_proposer",
                                     "sortition_params", "difficulty_bound"});
      test_params.block_proposer.lambda_bits = getConfigDataAsUInt(
          root,
          {"test_params", "block_proposer", "sortition_params", "lambda_bits"});
    } else {
      std::cerr << "Unknown propose mode: " << test_params.block_proposer.mode;
      assert(false);
    }
    test_params.pbft.lambda_ms_min =
        getConfigDataAsUInt(root, {"test_params", "pbft", "lambda_ms_min"});
    test_params.pbft.committee_size =
        getConfigDataAsUInt(root, {"test_params", "pbft", "committee_size"});
    test_params.pbft.valid_sortition_coins = getConfigDataAsUInt64(
        root, {"test_params", "pbft", "valid_sortition_coins"});
    test_params.pbft.dag_blocks_size =
        getConfigDataAsUInt(root, {"test_params", "pbft", "dag_blocks_size"});
    test_params.pbft.ghost_path_move_back = getConfigDataAsUInt(
        root, {"test_params", "pbft", "ghost_path_move_back"});
    test_params.pbft.skip_periods =
        getConfigDataAsUInt64(root, {"test_params", "pbft", "skip_periods"});
    test_params.pbft.run_count_votes =
        getConfigDataAsUInt(root, {"test_params", "pbft", "run_count_votes"});
  }

  if (!root["logging"].isNull()) {
    for (auto &item : root["logging"]["configurations"]) {
      auto on = getConfigDataAsBoolean(item, {"on"});
      if (on) {
        LoggingConfig logging;
        logging.name = getConfigDataAsString(item, {"name"});
        logging.verbosity =
            stringToVerbosity(getConfigDataAsString(item, {"verbosity"}));
        for (auto &ch : item["channels"]) {
          std::pair<std::string, uint16_t> channel;
          channel.first = getConfigDataAsString(ch, {"name"});
          if (ch["verbosity"].isNull()) {
            channel.second = logging.verbosity;
          } else {
            channel.second =
                stringToVerbosity(getConfigDataAsString(ch, {"verbosity"}));
          }
          logging.channels[channel.first] = channel.second;
        }
        for (auto &o : item["outputs"]) {
          LoggingOutputConfig output;
          output.type = getConfigDataAsString(o, {"type"});
          output.format = getConfigDataAsString(o, {"format"});
          if (output.type == "file") {
            output.file_name = getConfigDataAsString(o, {"file_name"});
            output.format = getConfigDataAsString(o, {"format"});
            output.max_size = getConfigDataAsUInt64(o, {"max_size"});
            output.rotation_size = getConfigDataAsUInt64(o, {"rotation_size"});
            output.time_based_rotation =
                getConfigDataAsString(o, {"time_based_rotation"});
          }
          logging.outputs.push_back(output);
        }
        log_configs.push_back(logging);
      }
    }
  }

  // TODO configurable
  opts_final_chain.state_api.ExpectedMaxNumTrxPerBlock = 400;
  // TODO constant
  opts_final_chain.state_api.MainTrieWriterOpts.ExpectedDepth = 64;
  opts_final_chain.state_api.MainTrieWriterOpts.FullNodeLevelsToCache = 5;
  opts_final_chain.state_api.AccTrieWriterOpts.ExpectedDepth = 16;
}

RpcConfig::RpcConfig(std::string const &json_file) : json_file_name(json_file) {
  try {
    Json::Value root;
    std::ifstream config_doc(json_file, std::ifstream::binary);
    config_doc >> root;
    port = root["port"].asUInt();
    ws_port = root["ws_port"].asUInt();
    address = boost::asio::ip::address::from_string(root["address"].asString());
  } catch (std::exception &e) {
    std::cerr << e.what() << std::endl;
  }
}

std::ostream &operator<<(std::ostream &strm, NodeConfig const &conf) {
  strm << "  [Node Config] " << std::endl;
  strm << "    node_id: " << conf.id << std::endl;
  strm << "    node_ip: " << conf.ip << std::endl;
  strm << "    node_port: " << conf.port << std::endl;
  return strm;
}

std::ostream &operator<<(std::ostream &strm, NetworkConfig const &conf) {
  strm << "[Network Config] " << std::endl;
  strm << "  json_file_name: " << conf.json_file_name << std::endl;
  strm << "  network_address: " << conf.network_address << std::endl;
  strm << "  network_listen_port: " << conf.network_listen_port << std::endl;
  strm << "  network_simulated_delay: " << conf.network_simulated_delay
       << std::endl;
  strm << "  network_transaction_interval: "
       << conf.network_transaction_interval << std::endl;
  strm << "  network_bandwidth: " << conf.network_bandwidth << std::endl;
  strm << "  network_ideal_peer_count: " << conf.network_ideal_peer_count
       << std::endl;
  strm << "  network_max_peer_count: " << conf.network_max_peer_count
       << std::endl;
  strm << "  network_sync_level_size: " << conf.network_sync_level_size
       << std::endl;
  strm << "  network_id: " << conf.network_id << std::endl;

  strm << "  --> boot nodes  ... " << std::endl;
  for (auto const &c : conf.network_boot_nodes) {
    strm << c << std::endl;
  }
  return strm;
}

std::ostream &operator<<(std::ostream &strm, FullNodeConfig const &conf) {
  strm << std::ifstream(conf.json_file_name).rdbuf() << std::endl;
  return strm;
}
}  // namespace taraxa