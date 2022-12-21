#include "config/config.hpp"

#include <json/json.h>

#include <fstream>

#include "common/jsoncpp.hpp"
#include "config/config_utils.hpp"

namespace taraxa {

void dec_json(Json::Value const &json, DBConfig &db_config) {
  db_config.db_snapshot_each_n_pbft_block =
      getConfigDataAsUInt(json, {"db_snapshot_each_n_pbft_block"}, true, db_config.db_snapshot_each_n_pbft_block);

  db_config.db_max_snapshots = getConfigDataAsUInt(json, {"db_max_snapshots"}, true, db_config.db_max_snapshots);
  db_config.db_max_open_files = getConfigDataAsUInt(json, {"db_max_open_files"}, true, db_config.db_max_open_files);
}

void FullNodeConfig::overwriteConfigFromJson(const Json::Value &root) {
  data_path = getConfigDataAsString(root, {"data_path"});
  db_path = data_path / "db";

  final_chain_cache_in_blocks =
      getConfigDataAsUInt(root, {"final_chain_cache_in_blocks"}, true, final_chain_cache_in_blocks);

  // config values that limits transactions and blocks memory pools
  transactions_pool_size = getConfigDataAsUInt(root, {"transactions_pool_size"}, true, kDefaultTransactionPoolSize);

  dec_json(root["network"], network);

  dec_json(root["db_config"], db_config);

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

  is_light_node = getConfigDataAsBoolean(root, {"is_light_node"}, true, is_light_node);
  light_node_history = getConfigDataAsUInt(root, {"light_node_history"}, true, light_node_history);
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
  overwriteConfigFromJson(root);

  if (const auto &v = genesis_json; v.isObject()) {
    dec_json(v, genesis);
  } else {
    genesis = Genesis();
  }

  is_light_node = getConfigDataAsBoolean(root, {"is_light_node"}, true, is_light_node);
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

void FullNodeConfig::validate() const {
  network.validate();
  genesis.validate();
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
  strm << "  collect_packets_stats: " << conf.collect_packets_stats << std::endl;
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
