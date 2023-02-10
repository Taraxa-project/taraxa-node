#include "cli/config.hpp"

#include <boost/algorithm/string.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <iostream>

#include "cli/config_updater.hpp"
#include "cli/tools.hpp"
#include "common/jsoncpp.hpp"
#include "config/version.hpp"
#include "config/config_utils.hpp"

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

Config::Config(int argc, const char* argv[]) {
  static constexpr const char* kNodeCommand = "node";
  static constexpr const char* kAccountCommand = "account";
  static constexpr const char* kVrfCommand = "vrf";
  static constexpr const char* kConfigCommand = "config";

  // Tmp variables for parsed command line options
  int chain_id = static_cast<int>(kDefaultChainId);

  std::string genesis_path_str, config_path_str, wallet_path_str, chain_str, node_secret, vrf_secret;
  std::vector<std::string> command, boot_nodes, log_channels, log_configurations, boot_nodes_append,
      log_channels_append;

  // Set node as default command
  command.push_back(kNodeCommand);

  bool overwrite_config, destroy_db, rebuild_network;
  overwrite_config = destroy_db = rebuild_network = false;

  // Define all command line options + descriptions
  // Add only such option to command line options that is not also part of config params
  boost::program_options::options_description node_cli_options("Node command line options");
  // Lambda for adding command line options
  auto addCliOption = node_cli_options.add_options();

  addCliOption("help", "Print this help message and exit");
  addCliOption("version", bpo::bool_switch(), "Print version of taraxad");
  addCliOption("command", bpo::value<std::vector<std::string>>(&command)->multitoken(),
               "Command arg:"
               "\nnode                  Runs the actual node (default)"
               "\nconfig       Only generate/overwrite config file with provided node command "
               "option without starting the node"
               "\naccount key           Generate new account or restore from a key (key is optional)"
               "\nvrf key               Generate new VRF or restore from a key (key is optional)");
  addCliOption("wallet", bpo::value<std::string>(&wallet_path_str),
               "JSON wallet file (default: \"~/.taraxa/wallet.json\")");
  addCliOption("config", bpo::value<std::string>(&config_path_str),
               "JSON configuration file (default: \"~/.taraxa/config.json\")");
  addCliOption("genesis", bpo::value<std::string>(&genesis_path_str),
               "JSON genesis file (default: \"~/.taraxa/genesis.json\")");
  addCliOption("destroy-db", bpo::bool_switch(&destroy_db), "Destroys all the existing data in the database");
  addCliOption("rebuild-network", bpo::bool_switch(&rebuild_network),
               "Delete all saved network/nodes information "
               "and rebuild network from boot nodes");
  addCliOption("chain", bpo::value<std::string>(&chain_str),
               "Chain identifier (string, mainnet, testnet, devnet) (default: mainnet) "
               "Only used when creating new config file");
  addCliOption("node-secret", bpo::value<std::string>(&node_secret), "Nose secret key to use");
  addCliOption("vrf-secret", bpo::value<std::string>(&vrf_secret), "Vrf secret key to use");
  addCliOption("chain-id", bpo::value<int>(&chain_id),
               "Chain identifier (integer, 841=Mainnet, 842=Testnet, 843=Devnet) (default: 841) "
               "Only used when creating new config file");
  addCliOption("boot-nodes", bpo::value<std::vector<std::string>>(&boot_nodes)->multitoken(),
               "Boot nodes to connect to: [ip_address:port_number/node_id, ....]");
  addCliOption("log-channels", bpo::value<std::vector<std::string>>(&log_channels)->multitoken(),
               "Log channels to log: [channel:level, ....]");
  addCliOption("log-configurations", bpo::value<std::vector<std::string>>(&log_configurations)->multitoken(),
               "Log confifugrations to use: [configuration_name, ....]");
  addCliOption(
    "boot-nodes-append", bpo::value<std::vector<std::string>>(&boot_nodes_append)->multitoken(),
    "Boot nodes to connect to in addition to boot nodes defined in config: [ip_address:port_number/node_id, ....]");
  addCliOption("log-channels-append", bpo::value<std::vector<std::string>>(&log_channels_append)->multitoken(),
               "Log channels to log in addition to log channels defined in config: [channel:level, ....]");
  addCliOption("overwrite-config", bpo::bool_switch(&overwrite_config),
               "Overwrite config - "
               "Multiple options like data_path, is_light_node, boot-nodes, log-channels, node-secret, vrf-secret, "
               "etc.. are always used in running a node but "
               "only written to config file if overwrite-config flag is set. \n"
               "WARNING: Overwrite-config set can override/delete current secret keys in the wallet");

  // Parse command line options
  bpo::variables_map option_vars;
  const auto parsed_cli_options = boost::program_options::parse_command_line(argc, argv, node_cli_options);
  boost::program_options::store(parsed_cli_options, option_vars);
  store(parsed_cli_options, option_vars);
  boost::program_options::notify(option_vars);

  // Update chain_id
  if (!chain_str.empty()) {
    if (chain_id != static_cast<int>(kDefaultChainId)) {
      throw ConfigException("Cannot specify both \"chain-d\" and \"chain\"");
    }
    chain_id = tools::getChainIdFromString(chain_str);
  }

  // Get files paths from cli options
  // Config file path
  if (config_path_str.empty()) {
    config_path_str = tools::getTaraxaDefaultConfigFile();
  }
  if (!fs::exists(config_path_str)) {
    std::cout << "Configuration file does not exist at: " << config_path_str
              << ". New default config file will be generated" << std::endl;
    util::writeJsonToFile(config_path_str, tools::getConfig(static_cast<Config::ChainIdType>(chain_id)));
  }

  // Genesis file path
  if (genesis_path_str.empty()) {
    genesis_path_str = tools::getTaraxaDefaultGenesisFile();
  }
  if (!fs::exists(genesis_path_str)) {
    std::cout << "Genesis file does not exist at: " << genesis_path_str
              << ". New default genesis file will be generated" << std::endl;
    util::writeJsonToFile(genesis_path_str, tools::getGenesis(static_cast<Config::ChainIdType>(chain_id)));
  }

  // Wallet file path
  if (wallet_path_str.empty()) {
    wallet_path_str = tools::getTaraxaDefaultWalletFile();
  }
  if (!fs::exists(wallet_path_str)) {
    std::cout << "Wallet file does not exist at: " << wallet_path_str << ". New wallet file will be generated"
              << std::endl;
    tools::generateWallet(wallet_path_str);
  }

  // Creates config object from parsed command & config options
  FullNodeConfig parsed_config = createFullNodeConfig(node_cli_options, option_vars, config_path_str, genesis_path_str, wallet_path_str);

  // Add options, which can be provided only through command line and not config
  addCliOnlyOptionsToConfig(parsed_config, boot_nodes, log_channels, log_configurations, boot_nodes_append, log_channels_append);

  if (option_vars.count("help")) {
    std::cout << "NAME:\n  "
                 "taraxad - Taraxa blockchain full node implementation\n"
                 "VERSION:\n  "
              << TARAXA_VERSION << "\nUSAGE:\n  taraxad [options]\n";
    std::cout << node_cli_options << std::endl;

    // If help message requested, ignore any additional commands
    command.clear();
    return;
  }

  if (option_vars.count("version")) {
    std::cout << kVersionJson << std::endl;
    // If version requested, ignore any additional commands
    command.clear();
    return;
  }

  if (command[0] == kNodeCommand || command[0] == kConfigCommand) {
    // Validate config values
    node_config_.validate();

    // TODO: seems like we can delete this one ?
    //    Json::Value config_json = util::readJsonFromFile(config_path_str);
    //    Json::Value genesis_json = util::readJsonFromFile(genesis_path_str);
    //    Json::Value wallet_json = util::readJsonFromFile(wallet_path_str);
    //
    //    auto write_config_and_wallet_files = [&]() {
    //      util::writeJsonToFile(config_path_str, config_json);
    //      util::writeJsonToFile(genesis_path_str, genesis_json);
    //      util::writeJsonToFile(wallet_path_str, wallet_json);
    //    };
    //
    //    // Check that it is not empty, to not create chain config with just overwritten files
    //    if (!genesis_json.isNull()) {
    //      auto tmp_genesis_json = tools::getGenesis((Config::ChainIdType)chain_id);
    //      // override hardforks data with one from default json
    //      addNewHardforks(genesis_json, tmp_genesis_json);
    //      // add vote_eligibility_balance_step field if it is missing in the config
    //      if (genesis_json["dpos"]["vote_eligibility_balance_step"].isNull()) {
    //        genesis_json["dpos"]["vote_eligibility_balance_step"] =
    //            tmp_genesis_json["dpos"]["vote_eligibility_balance_step"];
    //      }
    //      write_config_and_wallet_files();
    //    }

    // Override config values with values from CLI
    //    config_json = tools::overrideConfig(config_json, boot_nodes, log_channels, log_configurations,
    //                                        boot_nodes_append, log_channels_append);

    //wallet_json = tools::overrideWallet(wallet_json, node_secret, vrf_secret);

    // Create data directory
    if (!node_config_.data_path.empty() && !fs::exists(node_config_.data_path)) {
      fs::create_directories(node_config_.data_path);
    }

    //    {
    //      ConfigUpdater updater{chain_id};
    //      updater.UpdateConfig(config_json);
    //      write_config_and_wallet_files();
    //    }

    // Load config
    //node_config_ = FullNodeConfig(config_json, wallet_json, genesis_json, config_path_str);

    // Save changes permanently if overwrite_config option is set
    // or if running config command
    if (overwrite_config || command[0] == kConfigCommand) {
      // TODO: generate json from config object and write it to file
      //genesis_json = enc_json(node_config_.genesis);
      //write_config_and_wallet_files();
    }

    if (destroy_db) {
      fs::remove_all(node_config_.db_path);
    }
    if (rebuild_network) {
      fs::remove_all(node_config_.net_file_path());
    }

    if (command[0] == kNodeCommand) {
      node_configured_ = true;
    }

  } else if (command[0] == kAccountCommand) {
    if (command.size() == 1)
      tools::generateAccount();
    else
      tools::generateAccountFromKey(command[1]);
  } else if (command[0] == kVrfCommand) {
    if (command.size() == 1)
      tools::generateVrf();
    else
      tools::generateVrfFromKey(command[1]);
  } else {
    throw bpo::invalid_option_value(command[0]);
  }
}

FullNodeConfig Config::createFullNodeConfig(const boost::program_options::options_description& cli_options,
                                            boost::program_options::variables_map& option_vars,
                                            const std::string config_path,
                                            std::optional<std::string> genesis_path,
                                            std::optional<std::string> wallet_path) {
  FullNodeConfig new_config;

  // Define config options + descriptions
  boost::program_options::options_description config_options("Node config options");
  // Lambda for adding config options
  auto addConfigOption = config_options.add_options();

  std::string data_path_str;

  addConfigOption("data_path", bpo::value<std::string>(&data_path_str),
                  "Data path for the databases, logs ... (default: \"~/.taraxa/data\")");
  addConfigOption("final_chain_cache_in_blocks",
                  bpo::value<decltype(new_config.final_chain_cache_in_blocks)>(&new_config.final_chain_cache_in_blocks),
                  "Number of blocks kept in cache");
  addConfigOption("is_light_node", bpo::bool_switch(&new_config.is_light_node), "Enable light node functionality");
  addConfigOption("light_node_history",
                  bpo::value<decltype(new_config.light_node_history)>(&new_config.light_node_history),
                  "Number of periods to keep in history for a light node");
  addConfigOption("transactions_pool_size",
                  bpo::value<decltype(new_config.transactions_pool_size)>(&new_config.transactions_pool_size),
                  "Transaction pool size limit");
  addConfigOption("enable_test_rpc", bpo::bool_switch(&new_config.enable_test_rpc),
                  "Enables Test JsonRPC. Disabled by default");

  addConfigOption("network.listen_ip", bpo::value<decltype(new_config.network.listen_ip)>(&new_config.network.listen_ip),
                  "Listen IP (default: 127.0.0.1)");
  addConfigOption("network.public_ip", bpo::value<decltype(new_config.network.public_ip)>(&new_config.network.public_ip),
                  "Force advertised public IP to the given IP (default: auto)");
  addConfigOption("network.listen_port", bpo::value<decltype(new_config.network.listen_port)>(&new_config.network.listen_port),
                  "Listen on the given port for incoming connections");

  addConfigOption("network.transaction_interval_ms", bpo::value<decltype(new_config.network.transaction_interval_ms)>(&new_config.network.transaction_interval_ms),
                  "Interval [ms] for regular broadcasting of new transactions");
  addConfigOption("network.ideal_peer_count", bpo::value<decltype(new_config.network.ideal_peer_count)>(&new_config.network.ideal_peer_count),
                  "Ideal peer count");
  addConfigOption("network.max_peer_count", bpo::value<decltype(new_config.network.max_peer_count)>(&new_config.network.max_peer_count),
                  "Max peer count");
  addConfigOption("network.sync_level_size", bpo::value<decltype(new_config.network.sync_level_size)>(&new_config.network.sync_level_size),
                  "");
  addConfigOption("network.packets_processing_threads", bpo::value<decltype(new_config.network.packets_processing_threads)>(&new_config.network.packets_processing_threads),
                  "Number of threads for packets processing");
  addConfigOption("network.peer_blacklist_timeout", bpo::value<decltype(new_config.network.peer_blacklist_timeout)>(&new_config.network.peer_blacklist_timeout),
                  "How many seconds is peer blacklisted after it was marked ad malicious");
  addConfigOption("network.disable_peer_blacklist", bpo::bool_switch(&new_config.network.disable_peer_blacklist),
                  "Disable peers blacklisting (default: false)");
  addConfigOption("network.deep_syncing_threshold", bpo::value<decltype(new_config.network.deep_syncing_threshold)>(&new_config.network.deep_syncing_threshold),
                  "");

  addConfigOption("network.rpc.enabled", bpo::bool_switch(&new_config.network.rpc.enabled),
                  "Enable RPC to be called on node (default: false)");
  addConfigOption("network.rpc.threads_num",
                  bpo::value<decltype(new_config.network.rpc.threads_num)>(&new_config.network.rpc.threads_num),
                  "Number of threads reserved for RPC calls processing (default: N/A)");
  addConfigOption("network.rpc.http_port", bpo::value<uint16_t>(), "RPC http port (default: N/A)");
  addConfigOption("network.rpc.ws_port", bpo::value<uint16_t>(), "RPC websocket port (default: N/A)");

  addConfigOption("network.graphql.enabled", bpo::bool_switch(&new_config.network.graphql.enabled),
                  "Enable GraphQL to be called on node (default: false)");
  addConfigOption(
    "network.graphql.threads_num",
    bpo::value<decltype(new_config.network.graphql.threads_num)>(&new_config.network.graphql.threads_num),
    "Number of threads reserved for GraphQL calls processing (default: N/A)");
  addConfigOption("network.graphql.http_port", bpo::value<uint16_t>(), "GraphQL http port (default: N/A)");
  addConfigOption("network.graphql.ws_port", bpo::value<uint16_t>(), "GraphQL websocket port (default: N/A)");

  addConfigOption("network.prometheus.enabled", bpo::bool_switch(&new_config.network.prometheus.enabled),
                  "Enable Prometheus to be called on node (default: false)");
  addConfigOption(
    "network.prometheus.listen_port",
    bpo::value<decltype(new_config.network.prometheus.listen_port)>(&new_config.network.prometheus.listen_port),
    "Prometheus listen port (default: 0)");
  addConfigOption("network.prometheus.polling_interval_ms",
                  bpo::value<decltype(new_config.network.prometheus.polling_interval_ms)>(
                    &new_config.network.prometheus.polling_interval_ms),
                  "Prometheus http port (default: 1000)");

  addConfigOption(
    "network.ddos_protection.vote_accepting_periods",
    bpo::value<decltype(new_config.network.ddos_protection.vote_accepting_periods)>(&new_config.network.ddos_protection.vote_accepting_periods),
    "How many periods(of votes) into the future compared to the current state we accept");
  addConfigOption(
    "network.ddos_protection.vote_accepting_rounds",
    bpo::value<decltype(new_config.network.ddos_protection.vote_accepting_rounds)>(&new_config.network.ddos_protection.vote_accepting_rounds),
    "How many rounds(of votes) into the future compared to the current state we accept");
  addConfigOption(
    "network.ddos_protection.vote_accepting_steps",
    bpo::value<decltype(new_config.network.ddos_protection.vote_accepting_steps)>(&new_config.network.ddos_protection.vote_accepting_steps),
    "How many steps(of votes) into the future compared to the current state we accept");
  addConfigOption("network.ddos_protection.log_packets_stats", bpo::bool_switch(&new_config.network.ddos_protection.log_packets_stats),
                  "Log packets stats");
  addConfigOption(
    "network.ddos_protection.packets_stats_time_period_ms",
    bpo::value<decltype(new_config.network.ddos_protection.packets_stats_time_period_ms)>(&new_config.network.ddos_protection.packets_stats_time_period_ms),
    "Time period for collecting packets stats");
  addConfigOption(
    "network.ddos_protection.peer_max_packets_processing_time_us",
    bpo::value<decltype(new_config.network.ddos_protection.peer_max_packets_processing_time_us)>(&new_config.network.ddos_protection.peer_max_packets_processing_time_us),
    "Peer's max allowed packets processing time during packets_stats_time_period_ms");
  addConfigOption(
    "network.ddos_protection.peer_max_packets_queue_size_limit",
    bpo::value<decltype(new_config.network.ddos_protection.peer_max_packets_queue_size_limit)>(&new_config.network.ddos_protection.peer_max_packets_queue_size_limit),
    "Queue size limit when we start dropping packets from peers if they exceed peer_max_packets_processing_time_us");
  addConfigOption(
    "network.ddos_protection.max_packets_queue_size",
    bpo::value<decltype(new_config.network.ddos_protection.max_packets_queue_size)>(&new_config.network.ddos_protection.max_packets_queue_size),
    "Max packets queue size, 0 means unlimited");

  addConfigOption(
    "db_config.db_snapshot_each_n_pbft_block",
    bpo::value<decltype(new_config.db_config.db_snapshot_each_n_pbft_block)>(&new_config.db_config.db_snapshot_each_n_pbft_block),
    "Inerval [periods] for making database snapshots");
  addConfigOption(
    "db_config.db_max_snapshots",
    bpo::value<decltype(new_config.db_config.db_max_snapshots)>(&new_config.db_config.db_max_snapshots),
    "Mx number of database snapshots kept on disk");
  addConfigOption(
    "db_config.db_max_open_files",
    bpo::value<decltype(new_config.db_config.db_max_open_files)>(&new_config.db_config.db_max_open_files),
    "");
  addConfigOption("db_config.rebuild_db", bpo::bool_switch(&new_config.db_config.rebuild_db),
                  "Reads the raw dag/pbft blocks from the db and executes all the blocks from scratch "
                  "rebuilding all the other database tables - this could take a long time");
  addConfigOption("db_config.rebuild_db_columns", bpo::bool_switch(&new_config.db_config.rebuild_db_columns),
                  "Removes old DB columns ");
  addConfigOption("db_config.rebuild_db_period", bpo::value<uint64_t>(&new_config.db_config.rebuild_db_period),
                  "Use with rebuild-db - Rebuild db up to a specified period");
  addConfigOption("db_config.db_revert_to_period", bpo::value<uint64_t>(&new_config.db_config.db_revert_to_period),
                  "Revert db/state to specified period (specify period)");


  boost::program_options::options_description allowed_options("Allowed options");
  allowed_options.add(cli_options);
  allowed_options.add(config_options);

  // Parse config options
  const auto parsed_config_options = parseJsonConfigFile(std::ifstream(config_path), allowed_options);
  store(parsed_config_options, option_vars);
  boost::program_options::notify(option_vars);

  if (data_path_str.empty()) {
    new_config.data_path = fs::path(tools::getTaraxaDataDefaultDir());
  }
  new_config.db_path = new_config.data_path / "db";

  // Manually setup some config values based on parsed options
  if (option_vars.count("network.rpc.http_port")) {
    new_config.network.rpc.http_port = option_vars["network.rpc.http_port"].as<uint16_t>();
  }
  if (option_vars.count("network.rpc.ws_port")) {
    new_config.network.rpc.ws_port = option_vars["network.rpc.ws_port"].as<uint16_t>();
  }

  // network.graphql.ws_port
  if (option_vars.count("network.graphql.http_port")) {
    new_config.network.graphql.http_port = option_vars["network.graphql.http_port"].as<uint16_t>();
  }
  if (option_vars.count("network.graphql.ws_port")) {
    new_config.network.graphql.ws_port = option_vars["network.graphql.ws_port"].as<uint16_t>();
  }

  auto listen_ip = boost::asio::ip::address::from_string(new_config.network.listen_ip);
  new_config.network.prometheus.address = new_config.network.listen_ip;
  new_config.network.rpc.address = listen_ip;
  new_config.network.graphql.address = listen_ip;

  // TODO: load from config and set these values
  new_config.opts_final_chain.expected_max_trx_per_block = 1000;
  new_config.opts_final_chain.max_trie_full_node_levels_to_cache = 4;

  // Add unregistered config options values - genesis, wallet, logging config and boot nodes
  addUnregisteredConfigOptionsValues(new_config, config_path, genesis_path, wallet_path);

  return new_config;
}

void Config::addCliOnlyOptionsToConfig(FullNodeConfig& config,
                                       const std::vector<std::string>& override_boot_nodes,
                                       const std::vector<std::string>& append_boot_nodes,
                                       const std::vector<std::string>& enable_log_configurations,
                                       const std::vector<std::string>& override_log_channels,
                                       const std::vector<std::string>& append_log_channels) {
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
}

void Config::addUnregisteredConfigOptionsValues(FullNodeConfig& config, const std::string& config_path,
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
    for (auto &item : config_json["logging"]["configurations"]) {
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
            output.target = config.log_path;
            output.file_name = (config.log_path / getConfigDataAsString(o, {"file_name"})).string();
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
  }

  // Parse logging configurations
  for (auto &boot_node_json : config_json["network"]["boot_nodes"]) {
    NodeConfig boot_node_cfg;
    boot_node_cfg.id = getConfigDataAsString(boot_node_json, {"id"});
    boot_node_cfg.ip = getConfigDataAsString(boot_node_json, {"ip"});
    boot_node_cfg.port = getConfigDataAsUInt(boot_node_json, {"port"});

    config.network.boot_nodes.push_back(boot_node_cfg);
  }

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
      config.node_secret = dev::Secret(wallet_json["node_secret"].asString(),
                                             dev::Secret::ConstructFromStringType::FromHex);
      if (!wallet_json["node_public"].isNull()) {
        auto node_public = dev::Public(wallet_json["node_public"].asString(),
                                       dev::Public::ConstructFromStringType::FromHex);
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
    } catch (const dev::Exception &e) {
      throw ConfigException(std::string("Could not parse node_secret: ") + e.what());
    }

    try {
      config.vrf_secret = vrf_wrapper::vrf_sk_t(wallet_json["vrf_secret"].asString());
    } catch (const dev::Exception &e) {
      throw ConfigException(std::string("Could not parse vrf_secret: ") + e.what());
    }

    try {
      if (!wallet_json["vrf_public"].isNull()) {
        auto vrf_public = vrf_wrapper::vrf_pk_t(wallet_json["vrf_public"].asString());
        if (vrf_public != taraxa::vrf_wrapper::getVrfPublicKey(config.vrf_secret)) {
          throw ConfigException(std::string("Vrf secret key and public key in wallet do not match"));
        }
      }
    } catch (const dev::Exception &e) {
      throw ConfigException(std::string("Could not parse vrf_public: ") + e.what());
    }
  }
}

bool Config::nodeConfigured() const { return node_configured_; }

FullNodeConfig Config::getNodeConfiguration() const { return node_config_; }

void Config::addNewHardforks(Json::Value& genesis, const Json::Value& default_genesis) {
  auto& new_hardforks_json = default_genesis["hardforks"];
  auto& local_hardforks_json = genesis["hardforks"];

  if (local_hardforks_json.isNull()) {
    local_hardforks_json = new_hardforks_json;
    return;
  }
  for (auto itr = new_hardforks_json.begin(); itr != new_hardforks_json.end(); ++itr) {
    auto& local = local_hardforks_json[itr.key().asString()];
    if (local.isNull()) {
      local = itr->asString();
    }
  }
}

}  // namespace taraxa::cli
