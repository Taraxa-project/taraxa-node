#include "cli/config_parser.hpp"

#include <boost/algorithm/string.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <iostream>

#include "cli/config_updater.hpp"
#include "cli/tools.hpp"
#include "common/jsoncpp.hpp"
#include "config/config_utils.hpp"
#include "config/version.hpp"

namespace bpo = boost::program_options;

namespace taraxa::cli {

template <class charT>
void parseChildren(std::string prefix, boost::property_tree::ptree& tree,
                   boost::program_options::parsed_options& options) {
  if (tree.empty()) {
    // remove first dot
    std::basic_string<charT> name = prefix.substr(1);
    // remove last dot if present
    if (name.back() == '.') {
      name.pop_back();
    }
    std::basic_string<charT> value = tree.data();

    boost::program_options::basic_option<charT> opt;
    opt.string_key = name;
    opt.value.push_back(value);
    opt.unregistered = (options.description->find_nothrow(name, false) == nullptr);
    opt.position_key = -1;

    // append value to existing option-key if it exists
    for (auto& o : options.options) {
      if (o.string_key == name) {
        o.value.push_back(value);
        return;
      }
    }
    options.options.push_back(opt);
  } else {
    for (auto it = tree.begin(); it != tree.end(); ++it) {
      parseChildren<charT>(prefix + "." + it->first, it->second, options);
    }
  }
}

template <class charT>
void parseJsonOptions(std::basic_istream<charT>& is, boost::program_options::parsed_options& options) {
  boost::property_tree::basic_ptree<std::basic_string<charT>, std::basic_string<charT>> pt;
  boost::property_tree::read_json(is, pt);

  parseChildren<charT>(std::basic_string<charT>(), pt, options);
}

template <class charT>
boost::program_options::basic_parsed_options<charT> parseJsonConfigFile(
    std::basic_istream<charT>&& is, const boost::program_options::options_description& desc) {
  boost::program_options::parsed_options result(&desc);
  parseJsonOptions(is, result);

  return boost::program_options::basic_parsed_options<charT>(result);
}

boost::program_options::options_description ConfigParser::registerCliOnlyOptions() {
  // Add only such option to command line options that is not also part of config params
  boost::program_options::options_description cli_only_options("Node cli-only options");
  // Lambda for adding command line options
  auto addCliOption = cli_only_options.add_options();

  addCliOption("help", "Print this help message and exit");
  addCliOption("version", "Print version of taraxad");
  addCliOption("command", bpo::value<std::vector<std::string>>(&command_)->multitoken(),
               "Command arg:"
               "\nnode                  Runs the actual node (default)"
               "\nconfig       Only generate/overwrite config file with provided node command "
               "option without starting the node"
               "\naccount key           Generate new account or restore from a key (key is optional)"
               "\nvrf key               Generate new VRF or restore from a key (key is optional)");
  addCliOption("wallet", bpo::value<std::string>(&wallet_path_),
               "JSON wallet file (default: \"~/.taraxa/wallet.json\")");
  addCliOption("config", bpo::value<std::string>(&config_path_),
               "JSON configuration file (default: \"~/.taraxa/config.json\")");
  addCliOption("genesis", bpo::value<std::string>(&genesis_path_),
               "JSON genesis file (default: \"~/.taraxa/genesis.json\")");
  addCliOption("destroy-db", bpo::bool_switch(&destroy_db_), "Destroys all the existing data in the database");
  addCliOption("rebuild-network", bpo::bool_switch(&rebuild_network_),
               "Delete all saved network/nodes information "
               "and rebuild network from boot nodes");
  addCliOption("chain", bpo::value<std::string>(&chain_str_),
               "Chain identifier (string, mainnet, testnet, devnet) (default: mainnet) "
               "Only used when creating new config file");
  addCliOption("node-secret", bpo::value<std::string>(&node_secret_), "Node secret key to use");
  addCliOption("vrf-secret", bpo::value<std::string>(&vrf_secret_), "Vrf secret key to use");
  addCliOption("chain-id", bpo::value<int>(&chain_id_),
               "Chain identifier (integer, 841=Mainnet, 842=Testnet, 843=Devnet) (default: 841) "
               "Only used when creating new config file");
  addCliOption("boot-nodes", bpo::value<std::vector<std::string>>(&boot_nodes_)->multitoken(),
               "Boot nodes to connect to: [ip_address:port_number/node_id, ....]");
  addCliOption("log-channels", bpo::value<std::vector<std::string>>(&log_channels_)->multitoken(),
               "Log channels to log: [channel:level, ....]");
  addCliOption("log-configurations", bpo::value<std::vector<std::string>>(&log_configurations_)->multitoken(),
               "Log confifugrations to use: [configuration_name, ....]");
  addCliOption(
      "boot-nodes-append", bpo::value<std::vector<std::string>>(&boot_nodes_append_)->multitoken(),
      "Boot nodes to connect to in addition to boot nodes defined in config: [ip_address:port_number/node_id, ....]");
  addCliOption("log-channels-append", bpo::value<std::vector<std::string>>(&log_channels_append_)->multitoken(),
               "Log channels to log in addition to log channels defined in config: [channel:level, ....]");
  addCliOption("overwrite-config", bpo::bool_switch(&overwrite_config_),
               "Overwrite config - "
               "Multiple options like data_path, is_light_node, boot-nodes, log-channels, node-secret, vrf-secret, "
               "etc.. are always used in running a node but "
               "only written to config file if overwrite-config flag is set.");

  return cli_only_options;
}

boost::program_options::options_description ConfigParser::registerCliConfigOptions(FullNodeConfig& config) {
  // Define config options + descriptions
  boost::program_options::options_description cli_config_options("Node cli-config options");
  // Lambda for adding config options
  auto addConfigOption = cli_config_options.add_options();

  addConfigOption("data_path", bpo::value<std::string>(&data_path_str_),
                  "Data path for the databases, logs ... (default: \"~/.taraxa/data\")");
  addConfigOption("final_chain_cache_in_blocks",
                  bpo::value<decltype(config.final_chain_cache_in_blocks)>(&config.final_chain_cache_in_blocks),
                  "Number of blocks kept in cache");
  addConfigOption("is_light_node", bpo::bool_switch(&config.is_light_node), "Enable light node functionality");
  addConfigOption("light_node_history", bpo::value<decltype(config.light_node_history)>(&config.light_node_history),
                  "Number of periods to keep in history for a light node");
  addConfigOption("transactions_pool_size",
                  bpo::value<decltype(config.transactions_pool_size)>(&config.transactions_pool_size),
                  "Transaction pool size limit");
  addConfigOption("enable_test_rpc", bpo::bool_switch(&config.enable_test_rpc),
                  "Enables Test JsonRPC. Disabled by default");
  addConfigOption("enable_debug_rpc", bpo::bool_switch(&config.enable_debug_rpc),
                  "Enables Debug RPC interface. Disabled by default");

  addConfigOption("network.listen_ip", bpo::value<decltype(config.network.listen_ip)>(&config.network.listen_ip),
                  "Listen IP (default: 127.0.0.1)");
  addConfigOption("network.public_ip", bpo::value<decltype(config.network.public_ip)>(&config.network.public_ip),
                  "Force advertised public IP to the given IP (default: auto)");
  addConfigOption("network.listen_port", bpo::value<decltype(config.network.listen_port)>(&config.network.listen_port),
                  "Listen on the given port for incoming connections");

  addConfigOption("network.transaction_interval_ms",
                  bpo::value<decltype(config.network.transaction_interval_ms)>(&config.network.transaction_interval_ms),
                  "Interval [ms] for regular broadcasting of new transactions");
  addConfigOption("network.ideal_peer_count",
                  bpo::value<decltype(config.network.ideal_peer_count)>(&config.network.ideal_peer_count),
                  "Ideal peer count");
  addConfigOption("network.max_peer_count",
                  bpo::value<decltype(config.network.max_peer_count)>(&config.network.max_peer_count),
                  "Max peer count");
  addConfigOption("network.sync_level_size",
                  bpo::value<decltype(config.network.sync_level_size)>(&config.network.sync_level_size), "");
  addConfigOption(
      "network.packets_processing_threads",
      bpo::value<decltype(config.network.packets_processing_threads)>(&config.network.packets_processing_threads),
      "Number of threads for packets processing");
  addConfigOption("network.peer_blacklist_timeout",
                  bpo::value<decltype(config.network.peer_blacklist_timeout)>(&config.network.peer_blacklist_timeout),
                  "How many seconds is peer blacklisted after it was marked ad malicious");
  addConfigOption("network.disable_peer_blacklist", bpo::bool_switch(&config.network.disable_peer_blacklist),
                  "Disable peers blacklisting (default: false)");
  addConfigOption("network.deep_syncing_threshold",
                  bpo::value<decltype(config.network.deep_syncing_threshold)>(&config.network.deep_syncing_threshold),
                  "");

  addConfigOption("network.rpc.enabled", bpo::bool_switch(&config.network.rpc.enabled),
                  "Enable RPC to be called on node (default: false)");
  addConfigOption("network.rpc.threads_num",
                  bpo::value<decltype(config.network.rpc.threads_num)>(&config.network.rpc.threads_num),
                  "Number of threads reserved for RPC calls processing (default: N/A)");
  addConfigOption("network.rpc.http_port", bpo::value<uint16_t>(), "RPC http port (default: N/A)");
  addConfigOption("network.rpc.ws_port", bpo::value<uint16_t>(), "RPC websocket port (default: N/A)");

  addConfigOption("network.graphql.enabled", bpo::bool_switch(&config.network.graphql.enabled),
                  "Enable GraphQL to be called on node (default: false)");
  addConfigOption("network.graphql.threads_num",
                  bpo::value<decltype(config.network.graphql.threads_num)>(&config.network.graphql.threads_num),
                  "Number of threads reserved for GraphQL calls processing (default: N/A)");
  addConfigOption("network.graphql.http_port", bpo::value<uint16_t>(), "GraphQL http port (default: N/A)");
  addConfigOption("network.graphql.ws_port", bpo::value<uint16_t>(), "GraphQL websocket port (default: N/A)");

  addConfigOption("network.prometheus.enabled", bpo::bool_switch(&config.network.prometheus.enabled),
                  "Enable Prometheus to be called on node (default: false)");
  addConfigOption("network.prometheus.listen_port",
                  bpo::value<decltype(config.network.prometheus.listen_port)>(&config.network.prometheus.listen_port),
                  "Prometheus listen port (default: 0)");
  addConfigOption("network.prometheus.polling_interval_ms",
                  bpo::value<decltype(config.network.prometheus.polling_interval_ms)>(
                      &config.network.prometheus.polling_interval_ms),
                  "Prometheus http port (default: 1000)");

  addConfigOption("network.ddos_protection.vote_accepting_periods",
                  bpo::value<decltype(config.network.ddos_protection.vote_accepting_periods)>(
                      &config.network.ddos_protection.vote_accepting_periods),
                  "How many periods(of votes) into the future compared to the current state we accept");
  addConfigOption("network.ddos_protection.vote_accepting_rounds",
                  bpo::value<decltype(config.network.ddos_protection.vote_accepting_rounds)>(
                      &config.network.ddos_protection.vote_accepting_rounds),
                  "How many rounds(of votes) into the future compared to the current state we accept");
  addConfigOption("network.ddos_protection.vote_accepting_steps",
                  bpo::value<decltype(config.network.ddos_protection.vote_accepting_steps)>(
                      &config.network.ddos_protection.vote_accepting_steps),
                  "How many steps(of votes) into the future compared to the current state we accept");
  addConfigOption("network.ddos_protection.log_packets_stats",
                  bpo::bool_switch(&config.network.ddos_protection.log_packets_stats), "Log packets stats");
  addConfigOption("network.ddos_protection.packets_stats_time_period_ms",
                  bpo::value<decltype(packets_stats_time_period_ms_)>(&packets_stats_time_period_ms_),
                  "Time period for collecting packets stats");
  addConfigOption("network.ddos_protection.peer_max_packets_processing_time_us",
                  bpo::value<decltype(peer_max_packets_processing_time_us_)>(&peer_max_packets_processing_time_us_),
                  "Peer's max allowed packets processing time during packets_stats_time_period_ms");
  addConfigOption(
      "network.ddos_protection.peer_max_packets_queue_size_limit",
      bpo::value<decltype(config.network.ddos_protection.peer_max_packets_queue_size_limit)>(
          &config.network.ddos_protection.peer_max_packets_queue_size_limit),
      "Queue size limit when we start dropping packets from peers if they exceed peer_max_packets_processing_time_us");
  addConfigOption("network.ddos_protection.max_packets_queue_size",
                  bpo::value<decltype(config.network.ddos_protection.max_packets_queue_size)>(
                      &config.network.ddos_protection.max_packets_queue_size),
                  "Max packets queue size, 0 means unlimited");

  addConfigOption("db_config.db_snapshot_each_n_pbft_block",
                  bpo::value<decltype(config.db_config.db_snapshot_each_n_pbft_block)>(
                      &config.db_config.db_snapshot_each_n_pbft_block),
                  "Inerval [periods] for making database snapshots");
  addConfigOption("db_config.db_max_snapshots",
                  bpo::value<decltype(config.db_config.db_max_snapshots)>(&config.db_config.db_max_snapshots),
                  "Mx number of database snapshots kept on disk");
  addConfigOption("db_config.db_max_open_files",
                  bpo::value<decltype(config.db_config.db_max_open_files)>(&config.db_config.db_max_open_files), "");
  addConfigOption("db_config.rebuild_db", bpo::bool_switch(&config.db_config.rebuild_db),
                  "Reads the raw dag/pbft blocks from the db and executes all the blocks from scratch "
                  "rebuilding all the other database tables - this could take a long time");
  addConfigOption("db_config.rebuild_db_columns", bpo::bool_switch(&config.db_config.rebuild_db_columns),
                  "Removes old DB columns ");
  addConfigOption("db_config.rebuild_db_period", bpo::value<uint64_t>(&config.db_config.rebuild_db_period),
                  "Use with rebuild-db - Rebuild db up to a specified period");
  addConfigOption("db_config.db_revert_to_period", bpo::value<uint64_t>(&config.db_config.db_revert_to_period),
                  "Revert db/state to specified period (specify period)");

  return cli_config_options;
}

std::optional<FullNodeConfig> ConfigParser::createConfig(int argc, const char* argv[]) {
  static constexpr const char* kNodeCommand = "node";
  static constexpr const char* kAccountCommand = "account";
  static constexpr const char* kVrfCommand = "vrf";
  static constexpr const char* kConfigCommand = "config";

  // Newly created config based on cli arguments
  FullNodeConfig new_config;

  // Set node as default command
  command_.push_back(kNodeCommand);

  // Register cli-only as well as cli-config options
  boost::program_options::options_description allowed_options("Allowed options");
  const auto cli_only_options = registerCliOnlyOptions();
  allowed_options.add(cli_only_options);
  const auto cli_config_options = registerCliConfigOptions(new_config);
  allowed_options.add(cli_config_options);

  // Parse command line args
  bpo::variables_map option_vars;
  const auto parsed_cli_options = boost::program_options::parse_command_line(argc, argv, allowed_options);
  boost::program_options::store(parsed_cli_options, option_vars);
  store(parsed_cli_options, option_vars);
  boost::program_options::notify(option_vars);

  if (option_vars.count("help")) {
    std::cout << "NAME:\n  "
                 "taraxad - Taraxa blockchain full node implementation\n"
                 "VERSION:\n  "
              << TARAXA_VERSION << "\nUSAGE:\n";
    std::cout << "[cli only options]\"" << cli_only_options << std::endl;
    std::cout << "[cli config options]\"" << cli_config_options << std::endl;

    // If help message requested, ignore any additional commands
    return {};
  }

  if (option_vars.count("version")) {
    std::cout << kVersionJson << std::endl;

    // If version requested, ignore any additional commands
    return {};
  }

  // Update chain_id
  if (!chain_str_.empty()) {
    if (chain_id_ != static_cast<int>(kDefaultChainId)) {
      throw ConfigException("Cannot specify both \"chain-d\" and \"chain\"");
    }
    chain_id_ = tools::getChainIdFromString(chain_str_);
  }

  // Generate default paths and configs in case provided paths are either empty or they dont exist
  generateDefaultPathsAndConfigs(static_cast<ConfigParser::ChainIdType>(chain_id_), config_path_, genesis_path_,
                                 wallet_path_);

  // Parse json config files and update existing config object
  parseJsonConfigFiles(allowed_options, option_vars, new_config, config_path_, genesis_path_, wallet_path_);

  // Update/override config by options, which can be provided only through command line
  updateConfigFromCliSpecialOptions(new_config, boot_nodes_, log_channels_, log_configurations_, boot_nodes_append_,
                                    log_channels_append_, node_secret_, vrf_secret_);

  // Validate config values
  new_config.validate();

  // TODO: compare chain_id to genesis.chain_id

  if (command_[0] == kNodeCommand) {
    // Save changes permanently if overwrite_config option is set or if running config command
    if (overwrite_config_) {
      util::writeJsonToFile(config_path_, new_config.toJson());
      util::writeJsonToFile(wallet_path_, new_config.walletToJson());
    }

    if (destroy_db_) {
      fs::remove_all(new_config.db_path);
    }
    if (rebuild_network_) {
      fs::remove_all(new_config.net_file_path());
    }
    return new_config;

  } else if (command_[0] == kConfigCommand) {
    util::writeJsonToFile(config_path_, new_config.toJson());
    util::writeJsonToFile(wallet_path_, new_config.walletToJson());

  } else if (command_[0] == kAccountCommand) {
    command_.size() == 1 ? tools::generateAccount() : tools::generateAccountFromKey(command_[1]);
  } else if (command_[0] == kVrfCommand) {
    command_.size() == 1 ? tools::generateVrf() : tools::generateVrfFromKey(command_[1]);
  } else {
    throw bpo::invalid_option_value(command_[0]);
  }

  return {};
}

bool ConfigParser::updateConfigFromJson(FullNodeConfig& config, const std::string& json_config_path) {
  if (!fs::exists(json_config_path)) {
    std::cout << "ConfigParser::updateConfigFromJson: provided json_config_path does not exist: " << json_config_path;
    return false;
  }

  // Register cli-config options
  boost::program_options::options_description allowed_options("Allowed options");
  const auto cli_config_options = registerCliConfigOptions(config);
  allowed_options.add(cli_config_options);

  // Creates config object from parsed command & config options
  bpo::variables_map option_vars;
  parseJsonConfigFiles(allowed_options, option_vars, config, json_config_path);

  // Validate config values
  config.validate();

  return true;
}

bool ConfigParser::parseJsonConfigFiles(const boost::program_options::options_description& allowed_options,
                                        boost::program_options::variables_map& option_vars, FullNodeConfig& config,
                                        const std::string& json_config_path, std::optional<std::string> genesis_path,
                                        std::optional<std::string> wallet_path) {
  if (!fs::exists(json_config_path)) {
    std::cout << "Invalid config file path: " << json_config_path;
    return false;
  }

  // Parse config options
  const auto parsed_config_options = parseJsonConfigFile(std::ifstream(json_config_path), allowed_options);
  store(parsed_config_options, option_vars);
  boost::program_options::notify(option_vars);

  config.data_path = data_path_str_.empty() ? tools::getTaraxaDataDefaultDir() : data_path_str_;
  config.db_path = config.data_path / "db";

  // Manually setup some config values based on parsed options
  if (option_vars.count("network.rpc.http_port")) {
    config.network.rpc.http_port = option_vars["network.rpc.http_port"].as<uint16_t>();
  }
  if (option_vars.count("network.rpc.ws_port")) {
    config.network.rpc.ws_port = option_vars["network.rpc.ws_port"].as<uint16_t>();
  }

  // network.graphql.ws_port
  if (option_vars.count("network.graphql.http_port")) {
    config.network.graphql.http_port = option_vars["network.graphql.http_port"].as<uint16_t>();
  }
  if (option_vars.count("network.graphql.ws_port")) {
    config.network.graphql.ws_port = option_vars["network.graphql.ws_port"].as<uint16_t>();
  }

  auto listen_ip = boost::asio::ip::address::from_string(config.network.listen_ip);
  config.network.prometheus.address = config.network.listen_ip;
  config.network.rpc.address = listen_ip;
  config.network.graphql.address = listen_ip;

  config.network.ddos_protection.packets_stats_time_period_ms =
      std::chrono::milliseconds(packets_stats_time_period_ms_);
  config.network.ddos_protection.peer_max_packets_processing_time_us =
      std::chrono::microseconds(peer_max_packets_processing_time_us_);

  // TODO: load from config and set these values
  config.opts_final_chain.expected_max_trx_per_block = 1000;
  config.opts_final_chain.max_trie_full_node_levels_to_cache = 4;

  // Add unregistered config options values - genesis, wallet, logging config and boot nodes
  parseUnregisteredConfigOptionsValues(config, json_config_path, genesis_path, wallet_path);

  return true;
}

void ConfigParser::updateConfigFromCliSpecialOptions(FullNodeConfig& config,
                                                     const std::vector<std::string>& override_boot_nodes,
                                                     const std::vector<std::string>& append_boot_nodes,
                                                     const std::vector<std::string>& enable_log_configurations,
                                                     const std::vector<std::string>& override_log_channels,
                                                     const std::vector<std::string>& append_log_channels,
                                                     const std::string& node_secret, const std::string& vrf_secret) {
  auto parseBootNodeConfig = [](const std::string& boot_node_str) {
    std::vector<std::string> parsed_boot_node_str;
    boost::split(parsed_boot_node_str, boot_node_str, boost::is_any_of(":/"));
    if (parsed_boot_node_str.size() != 3) {
      throw ConfigException("Boot node in boot_nodes cli argument not specified correctly: " + boot_node_str);
    }

    NodeConfig boot_node_cfg;
    boot_node_cfg.id = parsed_boot_node_str[2];
    boot_node_cfg.ip = parsed_boot_node_str[0];
    boot_node_cfg.port = stoi(parsed_boot_node_str[1]);

    return boot_node_cfg;
  };

  // Override boot nodes
  if (!override_boot_nodes.empty()) {
    config.network.boot_nodes.clear();

    for (const auto& boot_node_str : override_boot_nodes) {
      const auto parsed_boot_node_cfg = parseBootNodeConfig(boot_node_str);
      config.network.boot_nodes.push_back(parsed_boot_node_cfg);
    }
  }

  // Append boot nodes
  if (!append_boot_nodes.empty()) {
    for (const auto& boot_node_str : append_boot_nodes) {
      const auto parsed_boot_node_cfg = parseBootNodeConfig(boot_node_str);

      // Check if such boot node does not already exist
      for (const auto& boot_node_cfg : config.network.boot_nodes) {
        if (boot_node_cfg.id == parsed_boot_node_cfg.id) {
          throw ConfigException("Cannot append boot node with id: " + parsed_boot_node_cfg.id + ". It already exists.");
        }
      }

      config.network.boot_nodes.push_back(parsed_boot_node_cfg);
    }
  }

  // Turn on logging configurations
  if (!enable_log_configurations.empty()) {
    for (const auto& enable_log_cfg : enable_log_configurations) {
      for (auto& existing_log_cfg : config.log_configs) {
        if (enable_log_cfg == existing_log_cfg.name) {
          existing_log_cfg.enabled = true;
        }
      }
    }
  }

  auto parseLogChannel = [](const std::string& log_channel_str) {
    std::vector<std::string> parsed_log_channel;
    boost::split(parsed_log_channel, log_channel_str, boost::is_any_of(":"));
    if (parsed_log_channel.size() != 2) {
      throw ConfigException("Log channel in log_channels not specified correctly");
    }

    return std::make_pair(parsed_log_channel[0], parsed_log_channel[1]);
  };

  // Override log channels
  if (!override_log_channels.empty()) {
    if (config.log_configs.empty()) {
      throw ConfigException("Cannot override log channels. No log configuration found.");
    }

    // TODO[2313]: choose logging config by specified name, not the first one as now
    auto& existing_logging_cfg = config.log_configs[0];
    existing_logging_cfg.channels.clear();

    for (const auto& log_channel : override_log_channels) {
      const auto [name, verbosity] = parseLogChannel(log_channel);
      existing_logging_cfg.channels[name] = logger::stringToVerbosity(verbosity);
    }
  }

  // Append log channels
  if (!append_log_channels.empty()) {
    if (config.log_configs.empty()) {
      throw ConfigException("Cannot append log channels. No log configuration found.");
    }

    // TODO[2313]: choose logging config by specified name, not the first one as now
    auto& existing_logging_cfg = config.log_configs[0];

    for (const auto& log_channel : append_log_channels) {
      const auto [name, verbosity] = parseLogChannel(log_channel);
      existing_logging_cfg.channels[name] = logger::stringToVerbosity(verbosity);
    }
  }

  if (!node_secret.empty()) {
    config.node_secret = dev::Secret(node_secret, dev::Secret::ConstructFromStringType::FromHex);
  }
  if (!vrf_secret.empty()) {
    config.vrf_secret = vrf_wrapper::vrf_sk_t(vrf_secret);
  }
}

void ConfigParser::parseUnregisteredConfigOptionsValues(FullNodeConfig& config, const std::string& config_path,
                                                        const std::optional<std::string>& genesis_path,
                                                        const std::optional<std::string>& wallet_path) {
  assert(!config_path.empty() && fs::exists(config_path));
  Json::Value config_json = util::readJsonFromFile(config_path);

  // Parse logging configurations
  // Network logging in p2p library creates performance issues even with
  // channel/verbosity off Disable it completely in net channel is not present
  if (!config_json["logging"].isNull()) {
    if (auto path = getConfigData(config_json["logging"], {"log_path"}, true); !path.isNull()) {
      config.log_path = path.asString();
    } else {
      config.log_path = config.data_path / "logs";
    }

    for (auto& item : config_json["logging"]["configurations"]) {
      logger::Config logging;
      logging.enabled = getConfigDataAsBoolean(item, {"on"});
      logging.name = getConfigDataAsString(item, {"name"});
      logging.verbosity = logger::stringToVerbosity(getConfigDataAsString(item, {"verbosity"}));
      for (auto& ch : item["channels"]) {
        std::pair<std::string, logger::Verbosity> channel;
        channel.first = getConfigDataAsString(ch, {"name"});
        if (ch["verbosity"].isNull()) {
          channel.second = logging.verbosity;
        } else {
          channel.second = logger::stringToVerbosity(getConfigDataAsString(ch, {"verbosity"}));
        }
        logging.channels[channel.first] = channel.second;
      }
      for (auto& o : item["outputs"]) {
        logger::Config::OutputConfig output;
        output.type = getConfigDataAsString(o, {"type"});
        output.format = getConfigDataAsString(o, {"format"});
        if (output.type == "file") {
          output.target = config.log_path;
          output.file_name = getConfigDataAsString(o, {"file_name"});
          output.format = getConfigDataAsString(o, {"format"});
          output.max_size = getConfigDataAsUInt64(o, {"max_size"});
          output.rotation_size = getConfigDataAsUInt64(o, {"rotation_size"});
          output.time_based_rotation = getConfigDataAsString(o, {"time_based_rotation"});
        }
        logging.outputs.push_back(output);
      }

      config.log_configs.push_back(logging);
    }
  }

  // Parse logging configurations
  for (auto& boot_node_json : config_json["network"]["boot_nodes"]) {
    NodeConfig boot_node_cfg;
    boot_node_cfg.id = getConfigDataAsString(boot_node_json, {"id"});
    boot_node_cfg.ip = getConfigDataAsString(boot_node_json, {"ip"});
    boot_node_cfg.port = getConfigDataAsUInt(boot_node_json, {"port"});

    config.network.boot_nodes.push_back(boot_node_cfg);
  }

  // TODO: if there is no genesis and wallet paths, generate default objects...
  // Parse genesis
  if (genesis_path.has_value()) {
    assert(!genesis_path->empty() && fs::exists(*genesis_path));
    Json::Value genesis_json = util::readJsonFromFile(*genesis_path);
    dec_json(genesis_json, config.genesis);
  }

  // Parse wallet
  if (wallet_path.has_value()) {
    assert(!wallet_path->empty() && fs::exists(*wallet_path));
    Json::Value wallet_json = util::readJsonFromFile(*wallet_path);

    try {
      config.node_secret =
          dev::Secret(wallet_json["node_secret"].asString(), dev::Secret::ConstructFromStringType::FromHex);
      if (!wallet_json["node_public"].isNull()) {
        auto node_public =
            dev::Public(wallet_json["node_public"].asString(), dev::Public::ConstructFromStringType::FromHex);
        if (node_public != dev::KeyPair(config.node_secret).pub()) {
          throw ConfigException(std::string("Node secret key and public key in wallet do not match"));
        }
      }
      if (!wallet_json["node_address"].isNull()) {
        auto node_address =
            dev::Address(wallet_json["node_address"].asString(), dev::Address::ConstructFromStringType::FromHex);
        if (node_address != dev::KeyPair(config.node_secret).address()) {
          throw ConfigException(std::string("Node secret key and address in wallet do not match"));
        }
      }
    } catch (const dev::Exception& e) {
      throw ConfigException(std::string("Could not parse node_secret: ") + e.what());
    }

    try {
      config.vrf_secret = vrf_wrapper::vrf_sk_t(wallet_json["vrf_secret"].asString());
    } catch (const dev::Exception& e) {
      throw ConfigException(std::string("Could not parse vrf_secret: ") + e.what());
    }

    try {
      if (!wallet_json["vrf_public"].isNull()) {
        auto vrf_public = vrf_wrapper::vrf_pk_t(wallet_json["vrf_public"].asString());
        if (vrf_public != taraxa::vrf_wrapper::getVrfPublicKey(config.vrf_secret)) {
          throw ConfigException(std::string("Vrf secret key and public key in wallet do not match"));
        }
      }
    } catch (const dev::Exception& e) {
      throw ConfigException(std::string("Could not parse vrf_public: ") + e.what());
    }
  }
}

void ConfigParser::generateDefaultPathsAndConfigs(ConfigParser::ChainIdType chain_id, std::string& config_path,
                                                  std::string& genesis_path, std::string& wallet_path) {
  // Config file path
  if (config_path.empty()) {
    config_path = tools::getTaraxaDefaultConfigFile();
  }
  if (!fs::exists(config_path)) {
    std::cout << "Configuration file does not exist at: " << config_path
              << ". New default config file will be generated" << std::endl;
    util::writeJsonToFile(config_path, tools::getConfig(chain_id));
  }

  // Genesis file path
  if (genesis_path.empty()) {
    genesis_path = tools::getTaraxaDefaultGenesisFile();
  }
  if (!fs::exists(genesis_path)) {
    std::cout << "Genesis file does not exist at: " << genesis_path << ". New default genesis file will be generated"
              << std::endl;
    util::writeJsonToFile(genesis_path, tools::getGenesis(chain_id));
  }

  // Wallet file path
  if (wallet_path.empty()) {
    wallet_path = tools::getTaraxaDefaultWalletFile();
  }
  if (!fs::exists(wallet_path)) {
    std::cout << "Wallet file does not exist at: " << wallet_path << ". New wallet file will be generated" << std::endl;
    tools::generateWallet(wallet_path);
  }
}

}  // namespace taraxa::cli
