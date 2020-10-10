#include "config.hpp"

//#include <Log.h>
#include <json/json.h>

#include <boost/algorithm/string.hpp>
#include <boost/core/null_deleter.hpp>
#include <fstream>

namespace taraxa {

using Logger = boost::log::sources::severity_channel_logger<>;

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

std::string getConfigDataAsString(Json::Value const &root,
                                  std::vector<string> const &path) {
  try {
    return getConfigData(root, path).asString();
  } catch (Json::Exception &e) {
    throw ConfigException(getConfigErr(path) + e.what());
  }
}

uint32_t getConfigDataAsUInt(Json::Value const &root,
                             std::vector<string> const &path,
                             bool optional = false, uint32_t value = 0) {
  try {
    Json::Value ret = getConfigData(root, path, optional);
    if (ret.isNull()) {
      return value;
    } else {
      return ret.asUInt();
    }
  } catch (Json::Exception &e) {
    if (optional) {
      return value;
    }
    throw ConfigException(getConfigErr(path) + e.what());
  }
}

uint64_t getConfigDataAsUInt64(Json::Value const &root,
                               std::vector<string> const &path) {
  try {
    return getConfigData(root, path).asUInt64();
  } catch (Json::Exception &e) {
    throw ConfigException(getConfigErr(path) + e.what());
  }
}

bool getConfigDataAsBoolean(Json::Value const &root,
                            std::vector<string> const &path) {
  try {
    return getConfigData(root, path).asBool();
  } catch (Json::Exception &e) {
    throw ConfigException(getConfigErr(path) + e.what());
  }
}

FullNodeConfig::FullNodeConfig(Json::Value const &string_or_object) : chain() {
  Json::Value parsed_from_file;
  if (string_or_object.isString()) {
    json_file_name = string_or_object.asString();
    std::ifstream config_doc(json_file_name, std::ifstream::binary);
    if (!config_doc.is_open()) {
      throw ConfigException(string("Could not open configuration file: ") +
                            json_file_name);
    }
    try {
      config_doc >> parsed_from_file;
    } catch (Json::Exception &e) {
      throw ConfigException(
          string("Could not parse json configuration file: ") + json_file_name +
          e.what());
    }
  }
  auto const &root =
      string_or_object.isString() ? parsed_from_file : string_or_object;
  node_secret = getConfigDataAsString(root, {"node_secret"});
  vrf_secret = getConfigDataAsString(root, {"vrf_secret"});
  db_path = getConfigDataAsString(root, {"db_path"});
  dag_processing_threads =
      getConfigDataAsUInt(root, {"dag_processing_threads"});

  network.network_address = getConfigDataAsString(root, {"network_address"});
  network.network_id = getConfigDataAsString(root, {"network_id"});
  network.network_tcp_port =
      getConfigDataAsUInt(root, {"network_tcp_port"});
  network.network_udp_port =
      getConfigDataAsUInt(root, {"network_udp_port"});
  network.network_simulated_delay =
      getConfigDataAsUInt(root, {"network_simulated_delay"});
  network.network_transaction_interval =
      getConfigDataAsUInt(root, {"network_transaction_interval"});
  network.network_min_dag_block_broadcast =
      getConfigDataAsUInt(root, {"network_min_dag_block_broadcast"}, true, 5);
  network.network_max_dag_block_broadcast =
      getConfigDataAsUInt(root, {"network_max_dag_block_broadcast"}, true, 20);
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
    node.udp_port = getConfigDataAsUInt(item, {"udp_port"});
    node.tcp_port = getConfigDataAsUInt(item, {"tcp_port"});
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
    test_params.block_proposer.min_proposal_delay = getConfigDataAsUInt(
        root, {"test_params", "block_proposer", "min_proposal_delay"});
    // VDF DAG proposal parameters
    test_params.block_proposer.difficulty_bound =
        getConfigDataAsUInt(root, {"test_params", "block_proposer",
                                   "sortition_params", "difficulty_bound"});
    test_params.block_proposer.lambda_bound = getConfigDataAsUInt(
        root,
        {"test_params", "block_proposer", "sortition_params", "lambda_bound"});

    test_params.pbft.lambda_ms_min =
        getConfigDataAsUInt(root, {"test_params", "pbft", "lambda_ms_min"});
    test_params.pbft.committee_size =
        getConfigDataAsUInt(root, {"test_params", "pbft", "committee_size"});
    test_params.pbft.dag_blocks_size =
        getConfigDataAsUInt(root, {"test_params", "pbft", "dag_blocks_size"});
    test_params.pbft.ghost_path_move_back = getConfigDataAsUInt(
        root, {"test_params", "pbft", "ghost_path_move_back"});
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
            output.file_name =
                db_path + "/" + getConfigDataAsString(o, {"file_name"});
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
  if (auto const &v = root["chain_config"]; v.isString()) {
    chain = ChainConfig::predefined(v.asString());
  } else if (v.isObject()) {
    dec_json(v, chain);
  } else {
    chain = ChainConfig::predefined();
  }
  // TODO configurable
  opts_final_chain.state_api.ExpectedMaxTrxPerBlock = 1000;
  opts_final_chain.state_api.MainTrieFullNodeLevelsToCache = 4;
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
  strm << "    node_udp_port: " << conf.udp_port << std::endl;
  strm << "    node_tcp_port: " << conf.tcp_port << std::endl;
  return strm;
}

std::ostream &operator<<(std::ostream &strm, NetworkConfig const &conf) {
  strm << "[Network Config] " << std::endl;
  strm << "  json_file_name: " << conf.json_file_name << std::endl;
  strm << "  network_address: " << conf.network_address << std::endl;
  strm << "  network_tcp_port: " << conf.network_tcp_port << std::endl;
  strm << "  network_udp_port: " << conf.network_udp_port << std::endl;
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