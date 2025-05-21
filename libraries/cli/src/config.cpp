#include "cli/config.hpp"

#include <libdevcore/CommonJS.h>

#include <iostream>

#include "cli/config_updater.hpp"
#include "cli/tools.hpp"
#include "common/jsoncpp.hpp"
#include "config/version.hpp"

namespace taraxa::cli {

Config::Config() : plugins_options_("PLUGINS") {}

void Config::addCliOptions(const bpo::options_description& options) { plugins_options_.add(options); }

void Config::parseCommandLine(int argc, const char* argv[], const std::string& available_plugins) {
  bpo::options_description allowed_options("");
  auto main_options = makeMainOptions();
  auto node_command_options = makeNodeOptions(available_plugins);
  allowed_options.add(main_options);
  allowed_options.add(node_command_options);
  allowed_options.add(plugins_options_);

  auto parsed_line = bpo::parse_command_line(argc, argv, allowed_options);

  bpo::store(parsed_line, cli_options_);
  bpo::notify(cli_options_);
  if (cli_options_.count(HELP)) {
    std::cout << "NAME:\n  "
                 "taraxad - Taraxa blockchain full node implementation\n"
                 "VERSION:\n  "
              << TARAXA_VERSION << "\nUSAGE:\n  taraxad [options]\n";
    std::cout << allowed_options << std::endl;
    // std::cout << node_command_options << std::endl;
    // If help message requested, ignore any additional commands
    return;
  }
  if (cli_options_.count(VERSION)) {
    std::cout << kVersionJson << std::endl;
    return;
  }
  if (cli_options_.count(PLUGINS)) {
    plugins_ = cli_options_[PLUGINS].as<std::vector<std::string>>();
  }

  std::vector<std::string> command;
  if (cli_options_.count(COMMAND)) {
    command = cli_options_[COMMAND].as<std::vector<std::string>>();
  }
  if (command.empty()) {
    command.push_back(NODE_COMMAND);
  }

  if (command[0] == NODE_COMMAND || command[0] == CONFIG_COMMAND) {
    // Create dir if missing
    auto config_dir = dirNameFromFile(config);
    auto wallet_dir = dirNameFromFile(wallet);
    auto genesis_dir = dirNameFromFile(genesis);
    if (!config_dir.empty() && !fs::exists(config_dir)) {
      fs::create_directories(config_dir);
    }
    if (!wallet_dir.empty() && !fs::exists(wallet_dir)) {
      fs::create_directories(wallet_dir);
    }
    if (!genesis_dir.empty() && !fs::exists(genesis_dir)) {
      fs::create_directories(genesis_dir);
    }

    // Update chain_id
    int chain_id = static_cast<int>(DEFAULT_CHAIN_ID);
    if (cli_options_.count(CHAIN_ID)) {
      if (cli_options_.count(CHAIN)) {
        std::cout << "You can not specify both " << CHAIN_ID << " and " << CHAIN << std::endl;
        return;
      }
      chain_id = cli_options_[CHAIN_ID].as<int>();
    }
    if (cli_options_.count(CHAIN)) {
      chain_id = tools::getChainIdFromString(cli_options_[CHAIN].as<std::string>());
    }

    // If any of the config files are missing they are generated with default values
    if (!fs::exists(config)) {
      std::cout << "Configuration file does not exist at: " << config << ". New config file will be generated"
                << std::endl;
      util::writeJsonToFile(config, tools::getConfig((Config::ChainIdType)chain_id));
    }
    if (!fs::exists(genesis)) {
      std::cout << "Genesis file does not exist at: " << genesis << ". New one file will be generated" << std::endl;
      util::writeJsonToFile(genesis, tools::getGenesis((Config::ChainIdType)chain_id));
    }
    if (!fs::exists(wallet)) {
      std::cout << "Wallet file does not exist at: " << wallet << ". New wallet file will be generated" << std::endl;
      tools::generateWallet(wallet);
    }

    Json::Value config_json = util::readJsonFromFile(config);
    Json::Value genesis_json = util::readJsonFromFile(genesis);
    Json::Value wallet_json = util::readJsonFromFile(wallet);

    auto write_config_and_wallet_files = [&]() {
      util::writeJsonToFile(config, config_json);
      util::writeJsonToFile(genesis, genesis_json);
      util::writeJsonToFile(wallet, wallet_json);
    };

    // Check that it is not empty, to not create chain config with just overwritten files
    if (!genesis_json.isNull()) {
      auto default_genesis_json = tools::getGenesis((Config::ChainIdType)genesis_json["chain_id"].asUInt64());
      // override hardforks data with one from default json
      genesis_json["hardforks"] = default_genesis_json["hardforks"];
      write_config_and_wallet_files();
    }
    // Override config values with values from CLI
    if (cli_options_.count(DATA_DIR)) {
      data_dir = cli_options_[DATA_DIR].as<std::string>();
    }
    std::vector<std::string> boot_nodes;
    if (cli_options_.count(BOOT_NODES)) {
      boot_nodes = cli_options_[BOOT_NODES].as<std::vector<std::string>>();
    }
    std::vector<std::string> log_channels;
    if (cli_options_.count(LOG_CHANNELS)) {
      log_channels = cli_options_[LOG_CHANNELS].as<std::vector<std::string>>();
    }
    std::vector<std::string> log_configurations;
    if (cli_options_.count(LOG_CONFIGURATIONS)) {
      log_configurations = cli_options_[LOG_CONFIGURATIONS].as<std::vector<std::string>>();
    }
    std::vector<std::string> boot_nodes_append;
    if (cli_options_.count(BOOT_NODES_APPEND)) {
      boot_nodes_append = cli_options_[BOOT_NODES_APPEND].as<std::vector<std::string>>();
    }
    std::vector<std::string> log_channels_append;
    if (cli_options_.count(LOG_CHANNELS_APPEND)) {
      log_channels_append = cli_options_[LOG_CHANNELS_APPEND].as<std::vector<std::string>>();
    }
    config_json = tools::overrideConfig(config_json, data_dir, boot_nodes, log_channels, log_configurations,
                                        boot_nodes_append, log_channels_append);

    std::string node_secret;
    if (cli_options_.count(NODE_SECRET)) {
      node_secret = cli_options_[NODE_SECRET].as<std::string>();
    }
    std::string vrf_secret;
    if (cli_options_.count(VRF_SECRET)) {
      vrf_secret = cli_options_[VRF_SECRET].as<std::string>();
    }
    wallet_json = tools::overrideWallet(wallet_json, node_secret, vrf_secret);

    config_json["is_light_node"] = cli_options_[LIGHT].as<bool>();
    // Create data directory
    if (!data_dir.empty() && !fs::exists(data_dir)) {
      fs::create_directories(data_dir);
    }

    {
      ConfigUpdater updater{chain_id};
      updater.UpdateConfig(config_json);
      write_config_and_wallet_files();
    }

    // Load config
    node_config_ = FullNodeConfig(config_json, wallet_json, genesis_json, config);

    // Save changes permanently if overwrite_config option is set
    // or if running config command
    // This can overwrite secret keys in wallet
    if (overwrite_config || command[0] == CONFIG_COMMAND) {
      genesis_json = enc_json(node_config_.genesis);
      write_config_and_wallet_files();
    }

    // Validate config values
    node_config_.validate();

    if (cli_options_[DESTROY_DB].as<bool>()) {
      fs::remove_all(node_config_.db_path);
    }
    if (cli_options_[REBUILD_NETWORK].as<bool>()) {
      fs::remove_all(node_config_.net_file_path());
    }
    if (cli_options_.count(PUBLIC_IP) && !cli_options_[PUBLIC_IP].as<std::string>().empty()) {
      node_config_.network.public_ip = cli_options_[PUBLIC_IP].as<std::string>();
    }
    if (cli_options_.count(PORT) && cli_options_[PORT].as<uint16_t>() != 0) {
      node_config_.network.listen_port = cli_options_[PORT].as<uint16_t>();
    }
    node_config_.db_config.db_revert_to_period = cli_options_[REVERT_TO_PERIOD].as<uint64_t>();
    node_config_.db_config.rebuild_db = cli_options_[REBUILD_DB].as<bool>();
    node_config_.db_config.prune_state_db = cli_options_[PRUNE_STATE_DB].as<bool>();
    node_config_.db_config.rebuild_db_period = cli_options_[REBUILD_DB_PERIOD].as<uint64_t>();
    node_config_.db_config.migrate_only = cli_options_[MIGRATE_ONLY].as<bool>();
    node_config_.db_config.migrate_receipts_by_period = cli_options_[MIGRATE_RECEIPTS_BY_PERIOD].as<bool>();

    if (command[0] == NODE_COMMAND) node_configured_ = true;
  } else if (command[0] == ACCOUNT_COMMAND) {
    if (command.size() == 1)
      tools::generateAccount();
    else
      tools::generateAccountFromKey(command[1]);
  } else if (command[0] == VRF_COMMAND) {
    if (command.size() == 1)
      tools::generateVrf();
    else
      tools::generateVrfFromKey(command[1]);
  } else {
    throw bpo::invalid_option_value(command[0]);
  }
}

bool Config::nodeConfigured() const { return node_configured_; }

FullNodeConfig Config::getNodeConfiguration() const { return node_config_; }

std::string Config::dirNameFromFile(const std::string& file) {
  size_t pos = file.find_last_of("\\/");
  return (std::string::npos == pos) ? "" : file.substr(0, pos);
}

bpo::options_description Config::makeMainOptions() {
  bpo::options_description main_options("OPTIONS");

  // Define all the command line options and descriptions
  main_options.add_options()(HELP, "Print this help message and exit");
  main_options.add_options()(VERSION, "Print version of taraxad");

  main_options.add_options()(COMMAND, bpo::value<std::vector<std::string>>()->multitoken(),
                             "Command arg:"
                             "\nnode                  Runs the actual node (default)"
                             "\nconfig       Only generate/overwrite config file with provided node command "
                             "option without starting the node"
                             "\naccount key           Generate new account or restore from a key (key is optional)"
                             "\nvrf key               Generate new VRF or restore from a key (key is optional)");
  return main_options;
}

bpo::options_description Config::makeNodeOptions(const std::string& available_plugins) {
  bpo::options_description node_command_options("NODE COMMAND OPTIONS");
  // Set config file and data directory to default values
  config = tools::getTaraxaDefaultConfigFile();
  wallet = tools::getTaraxaDefaultWalletFile();
  genesis = tools::getTaraxaDefaultGenesisFile();

  auto plugins_desc = "List of plugins to activate separated by space: " + available_plugins +
                      " (default: " + std::accumulate(plugins_.begin(), plugins_.end(), std::string()) + ")";
  node_command_options.add_options()(PLUGINS, bpo::value<std::vector<std::string>>()->multitoken(),
                                     plugins_desc.c_str());
  node_command_options.add_options()(WALLET, bpo::value<std::string>(&wallet),
                                     "JSON wallet file (default: \"~/.taraxa/wallet.json\")");
  node_command_options.add_options()(CONFIG, bpo::value<std::string>(&config),
                                     "JSON configuration file (default: \"~/.taraxa/config.json\")");
  node_command_options.add_options()(GENESIS, bpo::value<std::string>(&genesis),
                                     "JSON genesis file (default: \"~/.taraxa/genesis.json\")");
  node_command_options.add_options()(DATA_DIR, bpo::value<std::string>(&data_dir),
                                     "Data directory for the databases, logs ... (default: \"~/.taraxa/data\")");
  node_command_options.add_options()(LIGHT, bpo::bool_switch()->default_value(false),
                                     "Enable light node functionality");
  node_command_options.add_options()(CHAIN_ID, bpo::value<int>(),
                                     "Chain identifier (integer, 841=Mainnet, 842=Testnet, 843=Devnet) (default: 841) "
                                     "Only used when creating new config file");
  node_command_options.add_options()(CHAIN, bpo::value<std::string>(),
                                     "Chain identifier (string, mainnet, testnet, devnet) (default: mainnet) "
                                     "Only used when creating new config file");

  node_command_options.add_options()(BOOT_NODES, bpo::value<std::vector<std::string>>()->multitoken(),
                                     "Boot nodes to connect to: [ip_address:port_number/node_id, ....]");
  node_command_options.add_options()(
      BOOT_NODES_APPEND, bpo::value<std::vector<std::string>>()->multitoken(),
      "Boot nodes to connect to in addition to boot nodes defined in config: [ip_address:port_number/node_id, ....]");
  node_command_options.add_options()(PUBLIC_IP, bpo::value<std::string>(),
                                     "Force advertised public IP to the given IP (default: auto)");
  node_command_options.add_options()(PORT, bpo::value<uint16_t>(), "Listen on the given port for incoming connections");
  node_command_options.add_options()(LOG_CHANNELS, bpo::value<std::vector<std::string>>()->multitoken(),
                                     "Log channels to log: [channel:level, ....]");
  node_command_options.add_options()(
      LOG_CHANNELS_APPEND, bpo::value<std::vector<std::string>>()->multitoken(),
      "Log channels to log in addition to log channels defined in config: [channel:level, ....]");
  node_command_options.add_options()(LOG_CONFIGURATIONS, bpo::value<std::vector<std::string>>()->multitoken(),
                                     "Log configurations to use: [configuration_name, ....]");
  node_command_options.add_options()(NODE_SECRET, bpo::value<std::string>(), "Node secret key to use");

  node_command_options.add_options()(VRF_SECRET, bpo::value<std::string>(), "Vrf secret key to use");

  node_command_options.add_options()(
      OVERWRITE_CONFIG, bpo::bool_switch(&overwrite_config),
      "Overwrite config - "
      "Options data-dir, boot-nodes, log-channels, node-secret and vrf-secret are always used in running a node but "
      "only written to config file if overwrite-config flag is set. \n"
      "WARNING: Overwrite-config set can override/delete current secret keys in the wallet");

  // db related options
  node_command_options.add_options()(DESTROY_DB, bpo::bool_switch()->default_value(false),
                                     "Destroys all the existing data in the database");
  node_command_options.add_options()(REBUILD_DB, bpo::bool_switch()->default_value(false),
                                     "Reads the raw dag/pbft blocks from the db "
                                     "and executes all the blocks from scratch "
                                     "rebuilding all the other "
                                     "database tables - this could take a long "
                                     "time");
  node_command_options.add_options()(REBUILD_DB_PERIOD, bpo::value<uint64_t>()->default_value(0),
                                     "Use with rebuild-db - Rebuild db up "
                                     "to a specified period");
  node_command_options.add_options()(REBUILD_NETWORK, bpo::bool_switch()->default_value(false),
                                     "Delete all saved network/nodes information "
                                     "and rebuild network from boot nodes");
  node_command_options.add_options()(REVERT_TO_PERIOD, bpo::value<uint64_t>()->default_value(0),
                                     "Revert db/state to specified "
                                     "period (specify period)");
  node_command_options.add_options()(PRUNE_STATE_DB, bpo::bool_switch()->default_value(false), "Prune state_db");
  // migration related options
  node_command_options.add_options()(MIGRATE_ONLY, bpo::bool_switch()->default_value(false),
                                     "Only migrate DB, it will NOT run a node");
  node_command_options.add_options()(MIGRATE_RECEIPTS_BY_PERIOD, bpo::bool_switch()->default_value(false),
                                     "Apply migration to store receipts by period, not by hash");
  return node_command_options;
}

}  // namespace taraxa::cli
