#include "config/config.hpp"

#include <json/json.h>

#include <fstream>

#include "common/jsoncpp.hpp"

namespace taraxa {

std::string getConfigErr(std::vector<string> path) {
  std::string res = "Error in processing configuration file on param: ";
  for (size_t i = 0; i < path.size(); i++) res += path[i] + ".";
  res += " ";
  return res;
}

Json::Value getConfigData(Json::Value root, std::vector<string> const &path, bool optional = false) {
  for (size_t i = 0; i < path.size(); i++) {
    root = root[path[i]];
    if (root.isNull() && !optional) {
      throw ConfigException(getConfigErr(path) + "Element missing: " + path[i]);
    }
  }
  return root;
}

std::string getConfigDataAsString(Json::Value const &root, std::vector<string> const &path, bool optional = false,
                                  std::string value = {}) {
  try {
    Json::Value ret = getConfigData(root, path, optional);
    if (ret.isNull()) {
      return value;
    } else {
      return ret.asString();
    }
  } catch (Json::Exception &e) {
    if (optional) {
      return value;
    }
    throw ConfigException(getConfigErr(path) + e.what());
  }
}

uint32_t getConfigDataAsUInt(Json::Value const &root, std::vector<string> const &path, bool optional = false,
                             uint32_t value = 0) {
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

uint64_t getConfigDataAsUInt64(Json::Value const &root, std::vector<string> const &path) {
  try {
    return getConfigData(root, path).asUInt64();
  } catch (Json::Exception &e) {
    throw ConfigException(getConfigErr(path) + e.what());
  }
}

bool getConfigDataAsBoolean(Json::Value const &root, std::vector<string> const &path, bool optional = false,
                            bool value = false) {
  try {
    Json::Value ret = getConfigData(root, path, optional);
    if (ret.isNull()) {
      return value;
    } else {
      return ret.asBool();
    }
  } catch (Json::Exception &e) {
    if (optional) {
      return value;
    }
    throw ConfigException(getConfigErr(path) + e.what());
  }
}

Json::Value getJsonFromFileOrString(Json::Value const &value) {
  if (value.isString()) {
    std::string json_file_name = value.asString();
    if (!json_file_name.empty()) {
      std::ifstream config_doc(json_file_name, std::ifstream::binary);
      if (!config_doc.is_open()) {
        throw ConfigException(string("Could not open configuration file: ") + json_file_name);
      }
      try {
        Json::Value parsed_from_file;
        config_doc >> parsed_from_file;
        return parsed_from_file;
      } catch (Json::Exception &e) {
        throw ConfigException(string("Could not parse json configuration file: ") + json_file_name + e.what());
      }
    }
  }
  return value;
}

FullNodeConfig::FullNodeConfig(Json::Value const &string_or_object, Json::Value const &wallet,
                               const std::string &config_file_path) {
  Json::Value parsed_from_file = getJsonFromFileOrString(string_or_object);
  if (string_or_object.isString()) {
    json_file_name = string_or_object.asString();
  } else {
    json_file_name = config_file_path;
  }
  assert(!json_file_name.empty());
  auto const &root = string_or_object.isString() ? parsed_from_file : string_or_object;
  data_path = getConfigDataAsString(root, {"data_path"});
  db_path = data_path / "db";
  if (auto n = getConfigData(root, {"network_is_boot_node"}, true); !n.isNull()) {
    network.network_is_boot_node = n.asBool();
  }
  network.network_listen_ip = getConfigDataAsString(root, {"network_listen_ip"});
  network.network_public_ip = getConfigDataAsString(root, {"network_public_ip"}, true);
  network.network_tcp_port = getConfigDataAsUInt(root, {"network_tcp_port"});
  network.network_simulated_delay = getConfigDataAsUInt(root, {"network_simulated_delay"});
  network.network_performance_log_interval =
      getConfigDataAsUInt(root, {"network_performance_log_interval"}, true, 30000 /*ms*/);
  network.network_transaction_interval = getConfigDataAsUInt(root, {"network_transaction_interval"});
  network.network_bandwidth = getConfigDataAsUInt(root, {"network_bandwidth"});
  network.network_ideal_peer_count = getConfigDataAsUInt(root, {"network_ideal_peer_count"});
  network.network_max_peer_count = getConfigDataAsUInt(root, {"network_max_peer_count"});
  network.network_sync_level_size = getConfigDataAsUInt(root, {"network_sync_level_size"});
  network.network_packets_processing_threads = getConfigDataAsUInt(root, {"network_packets_processing_threads"});
  network.network_peer_blacklist_timeout = getConfigDataAsUInt(root, {"network_peer_blacklist_timeout"}, true,
                                                               NetworkConfig::kBlacklistTimeoutDefaultInSeconds);
  network.disable_peer_blacklist = getConfigDataAsBoolean(root, {"disable_peer_blacklist"}, true, false);
  network.deep_syncing_threshold =
      getConfigDataAsUInt(root, {"deep_syncing_threshold"}, true, network.deep_syncing_threshold);
  for (auto &item : root["network_boot_nodes"]) {
    NodeConfig node;
    node.id = getConfigDataAsString(item, {"id"});
    node.ip = getConfigDataAsString(item, {"ip"});
    node.udp_port = getConfigDataAsUInt(item, {"udp_port"});
    network.network_boot_nodes.push_back(node);
  }

  // Rpc config
  if (auto rpc_config = getConfigData(root, {"rpc"}, true); !rpc_config.isNull()) {
    rpc = RpcConfig();

    // ip address
    rpc->address = boost::asio::ip::address::from_string(network.network_listen_ip);

    // http port
    if (auto http_port = getConfigData(rpc_config, {"http_port"}, true); !http_port.isNull()) {
      rpc->http_port = http_port.asUInt();
    }

    // websocket port
    if (auto ws_port = getConfigData(rpc_config, {"ws_port"}, true); !ws_port.isNull()) {
      rpc->ws_port = ws_port.asUInt();
    }

    // number of threads processing rpc calls
    if (auto threads_num = getConfigData(rpc_config, {"threads_num"}, true); !threads_num.isNull()) {
      rpc->threads_num = threads_num.asUInt();
    }
  }

  {  // for test experiments
    test_params.max_transactions_pool_warn =
        getConfigDataAsUInt(root, {"test_params", "max_transactions_pool_warn"}, true);
    test_params.max_transactions_pool_drop =
        getConfigDataAsUInt(root, {"test_params", "max_transactions_pool_drop"}, true);

    test_params.max_block_queue_warn = getConfigDataAsUInt(root, {"test_params", "max_block_queue_warn"}, true);

    // Create db snapshot each N pbft block
    test_params.db_snapshot_each_n_pbft_block =
        getConfigDataAsUInt(root, {"test_params", "db_snapshot_each_n_pbft_block"}, true);

    test_params.db_max_snapshots = getConfigDataAsUInt(root, {"test_params", "db_max_snapshots"}, true);
    test_params.db_max_open_files = getConfigDataAsUInt(root, {"test_params", "db_max_open_files"}, true);

    // DAG proposal
    test_params.block_proposer.shard = getConfigDataAsUInt(root, {"test_params", "block_proposer", "shard"});
    test_params.block_proposer.transaction_limit =
        getConfigDataAsUInt(root, {"test_params", "block_proposer", "transaction_limit"});
  }

  // Network logging in p2p library creates performance issues even with
  // channel/verbosity off Disable it completely in net channel is not present
  if (!root["logging"].isNull()) {
    if (auto path = getConfigData(root["logging"], {"log_path"}, true); !path.isNull()) {
      log_path = path.asString();
    } else {
      log_path = data_path / "logs";
    }
    for (auto &item : root["logging"]["configurations"]) {
      auto on = getConfigDataAsBoolean(item, {"on"});
      if (on) {
        logger::Config logging;
        logging.name = getConfigDataAsString(item, {"name"});
        logging.verbosity = logger::stringToVerbosity(getConfigDataAsString(item, {"verbosity"}));
        for (auto &ch : item["channels"]) {
          std::pair<std::string, uint16_t> channel;
          channel.first = getConfigDataAsString(ch, {"name"});
          if (ch["verbosity"].isNull()) {
            channel.second = logging.verbosity;
          } else {
            channel.second = logger::stringToVerbosity(getConfigDataAsString(ch, {"verbosity"}));
          }
          logging.channels[channel.first] = channel.second;
        }
        for (auto &o : item["outputs"]) {
          logger::Config::OutputConfig output;
          output.type = getConfigDataAsString(o, {"type"});
          output.format = getConfigDataAsString(o, {"format"});
          if (output.type == "file") {
            output.target = log_path;
            output.file_name = (log_path / getConfigDataAsString(o, {"file_name"})).string();
            output.format = getConfigDataAsString(o, {"format"});
            output.max_size = getConfigDataAsUInt64(o, {"max_size"});
            output.rotation_size = getConfigDataAsUInt64(o, {"rotation_size"});
            output.time_based_rotation = getConfigDataAsString(o, {"time_based_rotation"});
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

  is_light_node = getConfigDataAsBoolean(root, {"is_light_node"}, true, false);
  if (is_light_node) {
    uint64_t min_light_node_history =
        (uint64_t)kDefaultLightNodeHistoryDays * 24 * 60 * 60000 / (chain.pbft.lambda_ms_min * 6);
    light_node_history = getConfigDataAsUInt(root, {"light_node_history"}, true, min_light_node_history);
    if (light_node_history < min_light_node_history) {
      light_node_history = min_light_node_history;
    }
  }

  node_secret = wallet["node_secret"].asString();
  vrf_secret = vrf_wrapper::vrf_sk_t(wallet["vrf_secret"].asString());

  network.network_id = chain.chain_id;
  // TODO configurable
  opts_final_chain.expected_max_trx_per_block = 1000;
  opts_final_chain.max_trie_full_node_levels_to_cache = 4;
}

void NetworkConfig::validate() const {
  if (network_sync_level_size == 0) {
    throw ConfigException(std::string("network_sync_level_size cannot be 0"));
  }

  // Max enabled number of threads for processing rpc requests
  constexpr uint16_t MAX_PACKETS_PROCESSING_THREADS_NUM = 30;
  if (network_packets_processing_threads < 3 ||
      network_packets_processing_threads > MAX_PACKETS_PROCESSING_THREADS_NUM) {
    throw ConfigException(std::string("network_packets_processing_threads must be in range [3, ") +
                          std::to_string(MAX_PACKETS_PROCESSING_THREADS_NUM) + "]");
  }

  if (network_transaction_interval < 0) {
    throw ConfigException(std::string("network_transaction_interval must be greater than zero"));
  }

  // TODO validate that the boot node list doesn't contain self (although it's not critical)
  for (auto const &node : network_boot_nodes) {
    if (node.ip.empty()) {
      throw ConfigException(std::string("Boot node ip is empty:") + node.ip + ":" + std::to_string(node.udp_port));
    }
    if (node.udp_port == 0) {
      throw ConfigException(std::string("Boot node port invalid: ") + std::to_string(node.udp_port));
    }
  }
}

void RpcConfig::validate() const {
  if (!http_port && !ws_port) {
    throw ConfigException("Either rpc::http_port or rpc::ws_port post must be specified for rpc");
  }

  // Max enabled number of threads for processing rpc requests
  constexpr uint16_t MAX_RPC_THREADS_NUM = 10;
  if (threads_num <= 0 || threads_num > MAX_RPC_THREADS_NUM) {
    throw ConfigException(string("rpc::threads_num must be in range (0, ") + std::to_string(MAX_RPC_THREADS_NUM) + "]");
  }
}

void FullNodeConfig::validate() const {
  network.validate();
  chain.validate();
  if (rpc) {
    rpc->validate();
  }
  // TODO: add validation of other config values
}

void FullNodeConfig::overwrite_chain_config_in_file() const {
  Json::Value from_file = getJsonFromFileOrString(json_file_name);
  from_file["chain_config"] = enc_json(chain);
  util::writeJsonToFile(json_file_name, from_file);
}

std::ostream &operator<<(std::ostream &strm, NodeConfig const &conf) {
  strm << "  [Node Config] " << std::endl;
  strm << "    node_id: " << conf.id << std::endl;
  strm << "    node_ip: " << conf.ip << std::endl;
  strm << "    node_udp_port: " << conf.udp_port << std::endl;
  return strm;
}

std::ostream &operator<<(std::ostream &strm, NetworkConfig const &conf) {
  strm << "[Network Config] " << std::endl;
  strm << "  json_file_name: " << conf.json_file_name << std::endl;
  strm << "  network_is_boot_node: " << conf.network_is_boot_node << std::endl;
  strm << "  network_listen_ip: " << conf.network_listen_ip << std::endl;
  strm << "  network_public_ip: " << conf.network_public_ip << std::endl;
  strm << "  network_tcp_port: " << conf.network_tcp_port << std::endl;
  strm << "  network_simulated_delay: " << conf.network_simulated_delay << std::endl;
  strm << "  network_transaction_interval: " << conf.network_transaction_interval << std::endl;
  strm << "  network_bandwidth: " << conf.network_bandwidth << std::endl;
  strm << "  network_ideal_peer_count: " << conf.network_ideal_peer_count << std::endl;
  strm << "  network_max_peer_count: " << conf.network_max_peer_count << std::endl;
  strm << "  network_sync_level_size: " << conf.network_sync_level_size << std::endl;
  strm << "  network_id: " << conf.network_id << std::endl;
  strm << "  network_performance_log_interval: " << conf.network_performance_log_interval << std::endl;
  strm << "  network_num_threads: " << conf.network_num_threads << std::endl;
  strm << "  network_packets_processing_threads: " << conf.network_packets_processing_threads << std::endl;
  strm << "  deep_syncing_threshold: " << conf.deep_syncing_threshold << std::endl;

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