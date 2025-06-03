#include "config/config.hpp"

#include <json/json.h>

#include <fstream>

#include "common/config_exception.hpp"
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

  blocks_gas_pricer = getConfigDataAsBoolean(root, {"blocks_gas_pricer"}, true, blocks_gas_pricer);

  dec_json(root["network"], network);
  dec_json(root["db_config"], db_config);
  dec_json(root["logging"], logging, data_path);

  is_light_node = getConfigDataAsBoolean(root, {"is_light_node"}, true, is_light_node);
  light_node_history = getConfigDataAsUInt(root, {"light_node_history"}, true, light_node_history);
  report_malicious_behaviour =
      getConfigDataAsUInt(root, {"report_malicious_behaviour"}, true, report_malicious_behaviour);
}

FullNodeConfig::FullNodeConfig(const Json::Value &string_or_object, const std::vector<Json::Value> &wallets_jsons,
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
    genesis = GenesisConfig();
  }

  propose_dag_gas_limit = getConfigDataAsUInt(root, {"propose_dag_gas_limit"}, true, propose_dag_gas_limit);
  propose_pbft_gas_limit = getConfigDataAsUInt(root, {"propose_pbft_gas_limit"}, true, propose_pbft_gas_limit);

  is_light_node = getConfigDataAsBoolean(root, {"is_light_node"}, true, is_light_node);
  const auto min_light_node_history = (genesis.state.dpos.blocks_per_year * kDefaultLightNodeHistoryDays) / 365;
  light_node_history = getConfigDataAsUInt(root, {"light_node_history"}, true, min_light_node_history);
  if (light_node_history < min_light_node_history) {
    throw ConfigException("Min. required light node history is " + std::to_string(min_light_node_history) +
                          " blocks (" + std::to_string(kDefaultLightNodeHistoryDays) + " days)");
  }

  for (const auto &wallet_json : wallets_jsons) {
    dev::Secret node_secret;
    vrf_wrapper::vrf_sk_t vrf_secret;

    try {
      node_secret = dev::Secret(wallet_json["node_secret"].asString(), dev::Secret::ConstructFromStringType::FromHex);
      if (!wallet_json["node_public"].isNull()) {
        auto node_public =
            dev::Public(wallet_json["node_public"].asString(), dev::Public::ConstructFromStringType::FromHex);
        if (node_public != dev::KeyPair(node_secret).pub()) {
          throw ConfigException(std::string("Node secret key and public key in wallet do not match"));
        }
      }
      if (!wallet_json["node_address"].isNull()) {
        auto node_address =
            dev::Address(wallet_json["node_address"].asString(), dev::Address::ConstructFromStringType::FromHex);
        if (node_address != dev::KeyPair(node_secret).address()) {
          throw ConfigException(std::string("Node secret key and address in wallet do not match"));
        }
      }
    } catch (const dev::Exception &e) {
      throw ConfigException(std::string("Could not parse node_secret: ") + e.what());
    }

    try {
      vrf_secret = vrf_wrapper::vrf_sk_t(wallet_json["vrf_secret"].asString());
    } catch (const dev::Exception &e) {
      throw ConfigException(std::string("Could not parse vrf_secret: ") + e.what());
    }

    try {
      if (!wallet_json["vrf_public"].isNull()) {
        auto vrf_public = vrf_wrapper::vrf_pk_t(wallet_json["vrf_public"].asString());
        if (vrf_public != taraxa::vrf_wrapper::getVrfPublicKey(vrf_secret)) {
          throw ConfigException(std::string("Vrf secret key and public key in wallet do not match"));
        }
      }
    } catch (const dev::Exception &e) {
      throw ConfigException(std::string("Could not parse vrf_public: ") + e.what());
    }

    auto wallet_config = WalletConfig(std::move(node_secret), vrf_secret);
    // Check for duplicate wallets
    if (std::any_of(wallets.cbegin(), wallets.cend(), [&wallet_config](const WalletConfig &wallet) {
          return wallet_config.node_secret == wallet.node_secret || wallet_config.vrf_secret == wallet.vrf_secret;
        })) {
      throw ConfigException(std::string("Duplicate wallets"));
    }

    wallets.push_back(std::move(wallet_config));
  }

  // TODO configurable
  opts_final_chain.expected_max_trx_per_block = 1000;
  opts_final_chain.max_trie_full_node_levels_to_cache = 4;
}

const WalletConfig &FullNodeConfig::getFirstWallet() const { return wallets.front(); }

void FullNodeConfig::validate() const {
  genesis.validate();
  network.validate(genesis.state.dpos.delegation_delay);

  if (transactions_pool_size < kMinTransactionPoolSize) {
    throw ConfigException("transactions_pool_size cannot be smaller than " + std::to_string(kMinTransactionPoolSize));
  }

  // TODO: add validation of other config values
}

std::string NodeConfig::toString() const {
  std::ostringstream strm;
  strm << "  [Node Config] " << std::endl;
  strm << "    node_id: " << id << std::endl;
  strm << "    node_ip: " << ip << std::endl;
  strm << "    node_udp_port: " << port << std::endl;
  return strm.str();
}

std::string DdosProtectionConfig::toString() const {
  std::ostringstream strm;
  strm << "  [Ddos protection] " << std::endl;
  strm << "    vote_accepting_periods: " << vote_accepting_periods << std::endl;
  strm << "    vote_accepting_rounds: " << vote_accepting_rounds << std::endl;
  strm << "    vote_accepting_steps: " << vote_accepting_steps << std::endl;
  strm << "    log_packets_stats: " << log_packets_stats << std::endl;
  strm << "    packets_stats_time_period_ms: " << packets_stats_time_period_ms.count() << std::endl;
  strm << "    peer_max_packets_processing_time_us: " << peer_max_packets_processing_time_us.count() << std::endl;
  strm << "    peer_max_packets_queue_size_limit: " << peer_max_packets_queue_size_limit << std::endl;
  strm << "    max_packets_queue_size: " << max_packets_queue_size << std::endl;
  return strm.str();
}

std::string NetworkConfig::toString() const {
  std::ostringstream strm;
  strm << "[Network Config] " << std::endl;
  strm << "  json_file_name: " << json_file_name << std::endl;
  strm << "  listen_ip: " << listen_ip << std::endl;
  strm << "  public_ip: " << public_ip << std::endl;
  strm << "  listen_port: " << listen_port << std::endl;
  strm << "  transaction_interval_ms: " << transaction_interval_ms << std::endl;
  strm << "  ideal_peer_count: " << ideal_peer_count << std::endl;
  strm << "  max_peer_count: " << max_peer_count << std::endl;
  strm << "  sync_level_size: " << sync_level_size << std::endl;
  strm << "  num_threads: " << num_threads << std::endl;
  strm << "  packets_processing_threads: " << packets_processing_threads << std::endl;
  strm << "  deep_syncing_threshold: " << deep_syncing_threshold << std::endl;
  strm << ddos_protection.toString() << std::endl;

  strm << "  --> boot nodes  ... " << std::endl;
  for (const auto &c : boot_nodes) {
    strm << c.toString() << std::endl;
  }
  return strm.str();
}

std::ostream &operator<<(std::ostream &strm, const FullNodeConfig &conf) {
  strm << std::ifstream(conf.json_file_name).rdbuf() << std::endl;
  return strm;
}

/**
 * @brief Transforms string verbosity to Verbosity enum
 *
 * @param verbosity
 * @return Verbosity enum
 */
spdlog::level::level_enum stringToVerbosity(std::string verbosity) {
  if (verbosity == "OFF") return spdlog::level::off;
  if (verbosity == "CRITICAL") return spdlog::level::critical;
  if (verbosity == "ERROR") return spdlog::level::err;
  if (verbosity == "WARN") return spdlog::level::warn;
  if (verbosity == "INFO") return spdlog::level::info;
  if (verbosity == "DEBUG") return spdlog::level::debug;
  if (verbosity == "TRACE") return spdlog::level::trace;
  throw ConfigException("Unknown verbosity string: " + verbosity);
}

/**
 * @brief Transforms string logging type to LoggingType enum
 *
 * @param logging_type string
 * @return LoggingType enum
 */
LoggingConfig::LoggingType stringToLoggingType(std::string logging_type) {
  if (logging_type == "console") return LoggingConfig::LoggingType::Console;
  if (logging_type == "file") return LoggingConfig::LoggingType::File;
  throw ConfigException("Unknown logging type string: " + logging_type);
}

void dec_json(const Json::Value &json, LoggingConfig &obj, std::filesystem::path data_path) {
  for (auto &ch : json["channels"]) {
    obj.channels[getConfigDataAsString(ch, {"name"})] = stringToVerbosity(getConfigDataAsString(ch, {"verbosity"}));
  }

  for (auto &o : json["outputs"]) {
    LoggingConfig::SinkConfig output;
    output.name = getConfigDataAsString(o, {"name"});
    output.type = stringToLoggingType(getConfigDataAsString(o, {"type"}));
    output.format = getConfigDataAsString(o, {"format"});
    output.verbosity = stringToVerbosity(getConfigDataAsString(o, {"verbosity"}));
    output.on = getConfigDataAsBoolean(o, {"on"});

    if (auto channels = getConfigData(json, {"channels"}, true); !channels.isNull()) {
      for (auto &ch : channels) {
        output.channels->push_back(ch.asString());
      }
    }

    if (output.type == LoggingConfig::LoggingType::File) {
      if (auto path = getConfigData(json, {"file_path"}, true); !path.isNull()) {
        output.file_path = path.asString();
      } else {
        output.file_path = data_path / "logs";
      }
      output.file_name = getConfigDataAsString(o, {"file_name"});
      output.file_full_path = *output.file_path / output.file_name;
      output.rotation_size = getConfigDataAsUInt(o, {"rotation_size"});
      output.max_files_num = getConfigDataAsUInt(o, {"max_files_num"});
    }

    obj.outputs.push_back(std::move(output));
  }
}

}  // namespace taraxa
