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

Json::Value getConfigData(Json::Value root, const std::vector<string> &path, bool optional = false) {
  for (size_t i = 0; i < path.size(); i++) {
    root = root[path[i]];
    if (root.isNull() && !optional) {
      throw ConfigException(getConfigErr(path) + "Element missing: " + path[i]);
    }
  }
  return root;
}

std::string getConfigDataAsString(const Json::Value &root, const std::vector<string> &path, bool optional = false,
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

uint32_t getConfigDataAsUInt(const Json::Value &root, const std::vector<string> &path, bool optional = false,
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

uint64_t getConfigDataAsUInt64(const Json::Value &root, const std::vector<string> &path) {
  try {
    return getConfigData(root, path).asUInt64();
  } catch (Json::Exception &e) {
    throw ConfigException(getConfigErr(path) + e.what());
  }
}

bool getConfigDataAsBoolean(const Json::Value &root, const std::vector<string> &path, bool optional = false,
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

Json::Value getJsonFromFileOrString(const Json::Value &value) {
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

NodeConfig dec_json(const Json::Value &json) {
  NodeConfig node;
  node.id = getConfigDataAsString(json, {"id"});
  node.ip = getConfigDataAsString(json, {"ip"});
  node.port = getConfigDataAsUInt(json, {"port"});
  return node;
}

void dec_json(const Json::Value &json, NetworkConfig &network) {
  network.listen_ip = getConfigDataAsString(json, {"listen_ip"});
  network.public_ip = getConfigDataAsString(json, {"public_ip"}, true);
  network.listen_port = getConfigDataAsUInt(json, {"listen_port"});
  network.performance_log_interval = getConfigDataAsUInt(json, {"performance_log_interval"}, true, 30000 /*ms*/);
  network.transaction_interval_ms = getConfigDataAsUInt(json, {"transaction_interval_ms"});
  network.ideal_peer_count = getConfigDataAsUInt(json, {"ideal_peer_count"});
  network.max_peer_count = getConfigDataAsUInt(json, {"max_peer_count"});
  network.sync_level_size = getConfigDataAsUInt(json, {"sync_level_size"});
  network.packets_processing_threads = getConfigDataAsUInt(json, {"packets_processing_threads"});
  network.peer_blacklist_timeout =
      getConfigDataAsUInt(json, {"peer_blacklist_timeout"}, true, NetworkConfig::kBlacklistTimeoutDefaultInSeconds);
  network.disable_peer_blacklist = getConfigDataAsBoolean(json, {"disable_peer_blacklist"}, true, false);
  network.deep_syncing_threshold =
      getConfigDataAsUInt(json, {"deep_syncing_threshold"}, true, network.deep_syncing_threshold);
  network.vote_accepting_periods =
      getConfigDataAsUInt(json, {"vote_accepting_periods"}, true, network.vote_accepting_periods);
  network.vote_accepting_rounds =
      getConfigDataAsUInt(json, {"vote_accepting_rounds"}, true, network.vote_accepting_rounds);
  network.vote_accepting_steps =
      getConfigDataAsUInt(json, {"vote_accepting_steps"}, true, network.vote_accepting_steps);
  for (auto &item : json["boot_nodes"]) {
    network.boot_nodes.push_back(dec_json(item));
  }
}

FullNodeConfig::FullNodeConfig(const Json::Value &string_or_object, const Json::Value &wallet,
                               const Json::Value &genesis_json, const std::string &config_file_path) {
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
  chain_id = getConfigDataAsUInt(root, {"chain_id"});
  final_chain_cache_in_blocks =
      getConfigDataAsUInt(root, {"final_chain_cache_in_blocks"}, true, final_chain_cache_in_blocks);

  dec_json(root["network"], network);

  // Rpc config
  if (auto rpc_config = getConfigData(root, {"rpc"}, true); !rpc_config.isNull()) {
    rpc = ConnectionConfig();

    // ip address
    rpc->address = boost::asio::ip::address::from_string(network.listen_ip);

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

  // GraphQL config
  if (auto graphql_config = getConfigData(root, {"graphql"}, true); !graphql_config.isNull()) {
    graphql = ConnectionConfig();

    // ip address
    graphql->address = boost::asio::ip::address::from_string(network.listen_ip);

    // graphql http port
    if (auto http_port = getConfigData(graphql_config, {"http_port"}, true); !http_port.isNull()) {
      graphql->http_port = http_port.asUInt();
    }

    // graphql websocket port
    if (auto ws_port = getConfigData(graphql_config, {"ws_port"}, true); !ws_port.isNull()) {
      graphql->ws_port = ws_port.asUInt();
    }

    // number of threads processing rpc calls
    if (auto threads_num = getConfigData(graphql_config, {"threads_num"}, true); !threads_num.isNull()) {
      graphql->threads_num = threads_num.asUInt();
    }
  }

  // config values that limits transactions and blocks memory pools
  transactions_pool_size = getConfigDataAsUInt(root, {"transactions_pool_size"}, true, kDefaultTransactionPoolSize);

  {  // db_config
    // Create db snapshot each N pbft block
    db_config.db_snapshot_each_n_pbft_block =
        getConfigDataAsUInt(root, {"db_config", "db_snapshot_each_n_pbft_block"}, true);

    db_config.db_max_snapshots = getConfigDataAsUInt(root, {"db_config", "db_max_snapshots"}, true);
    db_config.db_max_open_files = getConfigDataAsUInt(root, {"db_config", "db_max_open_files"}, true);
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
  if (const auto &v = genesis_json; v.isObject()) {
    dec_json(v, genesis);
  } else {
    genesis = Genesis::predefined();
  }

  // blocks_per_year config param is calculated from lambda_ms
  uint64_t year_ms = 365 * 24 * 60 * 60;
  year_ms *= 1000;
  // we have fixed 2*lambda time for proposing step and adding some expecting value for filter and certify steps
  const uint32_t expected_block_time = 2 * genesis.pbft.lambda_ms + 700;
  genesis.state.dpos.blocks_per_year = year_ms / expected_block_time;

  is_light_node = getConfigDataAsBoolean(root, {"is_light_node"}, true, false);
  if (is_light_node) {
    const auto min_light_node_history = (genesis.state.dpos.blocks_per_year * kDefaultLightNodeHistoryDays) / 365;
    light_node_history = getConfigDataAsUInt(root, {"light_node_history"}, true, min_light_node_history);
    if (light_node_history < min_light_node_history) {
      throw ConfigException("Min. required light node history is " + std::to_string(min_light_node_history) +
                            " blocks (" + std::to_string(kDefaultLightNodeHistoryDays) + " days)");
    }
  }

  try {
    node_secret = dev::Secret(wallet["node_secret"].asString(), dev::Secret::ConstructFromStringType::FromHex);
    if (!wallet["node_public"].isNull()) {
      auto node_public = dev::Public(wallet["node_public"].asString(), dev::Public::ConstructFromStringType::FromHex);
      if (node_public != dev::KeyPair(node_secret).pub()) {
        throw ConfigException(std::string("Node secret key and public key in wallet do not match"));
      }
    }
    if (!wallet["node_address"].isNull()) {
      auto node_address =
          dev::Address(wallet["node_address"].asString(), dev::Address::ConstructFromStringType::FromHex);
      if (node_address != dev::KeyPair(node_secret).address()) {
        throw ConfigException(std::string("Node secret key and address in wallet do not match"));
      }
    }
  } catch (const dev::Exception &e) {
    throw ConfigException(std::string("Could not parse node_secret: ") + e.what());
  }

  try {
    vrf_secret = vrf_wrapper::vrf_sk_t(wallet["vrf_secret"].asString());
  } catch (const dev::Exception &e) {
    throw ConfigException(std::string("Could not parse vrf_secret: ") + e.what());
  }

  try {
    if (!wallet["vrf_public"].isNull()) {
      auto vrf_public = vrf_wrapper::vrf_pk_t(wallet["vrf_public"].asString());
      if (vrf_public != taraxa::vrf_wrapper::getVrfPublicKey(vrf_secret)) {
        throw ConfigException(std::string("Vrf secret key and public key in wallet do not match"));
      }
    }
  } catch (const dev::Exception &e) {
    throw ConfigException(std::string("Could not parse vrf_public: ") + e.what());
  }
  // TODO configurable
  opts_final_chain.expected_max_trx_per_block = 1000;
  opts_final_chain.max_trie_full_node_levels_to_cache = 4;
}

void NetworkConfig::validate() const {
  if (sync_level_size == 0) {
    throw ConfigException(std::string("network.sync_level_size cannot be 0"));
  }

  // Max enabled number of threads for processing rpc requests
  constexpr uint16_t MAX_PACKETS_PROCESSING_THREADS_NUM = 30;
  if (packets_processing_threads < 3 || packets_processing_threads > MAX_PACKETS_PROCESSING_THREADS_NUM) {
    throw ConfigException(std::string("network.packets_processing_threads must be in range [3, ") +
                          std::to_string(MAX_PACKETS_PROCESSING_THREADS_NUM) + "]");
  }

  if (transaction_interval_ms == 0) {
    throw ConfigException(std::string("network.transaction_interval_ms must be greater than zero"));
  }

  // TODO validate that the boot node list doesn't contain self (although it's not critical)
  for (const auto &node : boot_nodes) {
    if (node.ip.empty()) {
      throw ConfigException(std::string("Boot node ip is empty:") + node.ip + ":" + std::to_string(node.port));
    }
    if (node.port == 0) {
      throw ConfigException(std::string("Boot node port invalid: ") + std::to_string(node.port));
    }
  }
}

void ConnectionConfig::validate() const {
  if (!http_port && !ws_port) {
    throw ConfigException("Either http_port or ws_port post must be specified for connection config");
  }

  // Max enabled number of threads for processing rpc requests
  constexpr uint16_t MAX_RPC_THREADS_NUM = 10;
  if (threads_num <= 0 || threads_num > MAX_RPC_THREADS_NUM) {
    throw ConfigException(string("threads_num must be in range (0, ") + std::to_string(MAX_RPC_THREADS_NUM) + "]");
  }
}

void FullNodeConfig::validate() const {
  network.validate();
  genesis.validate();
  if (rpc) {
    rpc->validate();
  }
  if (network.vote_accepting_periods > genesis.state.dpos.delegation_delay) {
    throw ConfigException(
        std::string("network.vote_accepting_periods(" + std::to_string(network.vote_accepting_periods) +
                    ") must be <= DPOS.delegation_delay(" + std::to_string(genesis.state.dpos.delegation_delay) + ")"));
  }
  if (transactions_pool_size < kMinTransactionPoolSize) {
    throw ConfigException("transactions_pool_size cannot be smaller than " + std::to_string(kMinTransactionPoolSize) +
                          ".");
  }

  // TODO: add validation of other config values
}

std::ostream &operator<<(std::ostream &strm, const NodeConfig &conf) {
  strm << "  [Node Config] " << std::endl;
  strm << "    node_id: " << conf.id << std::endl;
  strm << "    node_ip: " << conf.ip << std::endl;
  strm << "    node_udp_port: " << conf.port << std::endl;
  return strm;
}

std::ostream &operator<<(std::ostream &strm, const NetworkConfig &conf) {
  strm << "[Network Config] " << std::endl;
  strm << "  json_file_name: " << conf.json_file_name << std::endl;
  strm << "  listen_ip: " << conf.listen_ip << std::endl;
  strm << "  public_ip: " << conf.public_ip << std::endl;
  strm << "  listen_port: " << conf.listen_port << std::endl;
  strm << "  transaction_interval_ms: " << conf.transaction_interval_ms << std::endl;
  strm << "  ideal_peer_count: " << conf.ideal_peer_count << std::endl;
  strm << "  max_peer_count: " << conf.max_peer_count << std::endl;
  strm << "  sync_level_size: " << conf.sync_level_size << std::endl;
  strm << "  performance_log_interval: " << conf.performance_log_interval << std::endl;
  strm << "  num_threads: " << conf.num_threads << std::endl;
  strm << "  packets_processing_threads: " << conf.packets_processing_threads << std::endl;
  strm << "  deep_syncing_threshold: " << conf.deep_syncing_threshold << std::endl;

  strm << "  --> boot nodes  ... " << std::endl;
  for (const auto &c : conf.boot_nodes) {
    strm << c << std::endl;
  }
  return strm;
}

std::ostream &operator<<(std::ostream &strm, const FullNodeConfig &conf) {
  strm << std::ifstream(conf.json_file_name).rdbuf() << std::endl;
  return strm;
}
}  // namespace taraxa
