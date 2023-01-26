#include "cli/config.hpp"

#include <libdevcore/CommonJS.h>

#include <iostream>

#include "cli/config_updater.hpp"
#include "cli/tools.hpp"
#include "common/jsoncpp.hpp"
#include "config/version.hpp"

namespace bpo = boost::program_options;

namespace taraxa::cli {

Config::Config(int argc, const char* argv[]) {
  boost::program_options::options_description main_options("OPTIONS");
  boost::program_options::options_description node_command_options("NODE COMMAND OPTIONS");
  boost::program_options::options_description allowed_options("Allowed options");
  std::string genesis;
  std::string config;
  std::string wallet;
  int chain_id = static_cast<int>(DEFAULT_CHAIN_ID);
  std::string chain_str;
  std::string data_dir;
  std::vector<std::string> command;
  std::vector<std::string> boot_nodes;
  std::string public_ip;
  uint16_t port = 0;
  std::vector<std::string> log_channels;
  std::vector<std::string> log_configurations;
  std::vector<std::string> boot_nodes_append;
  std::vector<std::string> log_channels_append;
  std::string node_secret;
  std::string vrf_secret;
  bool overwrite_config;

  bool destroy_db = false;
  bool rebuild_network = false;
  bool rebuild_db = false;
  bool rebuild_db_columns = false;
  bool light_node = false;
  bool version = false;
  uint64_t rebuild_db_period = 0;
  uint64_t revert_to_period = 0;

  bool enable_test_rpc = false;
  bool enable_debug = false;

  // Set node as default command
  command.push_back(NODE_COMMAND);

  // Set config file and data directory to default values
  config = tools::getTaraxaDefaultConfigFile();
  wallet = tools::getTaraxaDefaultWalletFile();
  genesis = tools::getTaraxaDefaultGenesisFile();

  // Define all the command line options and descriptions
  main_options.add_options()(HELP, "Print this help message and exit");
  main_options.add_options()(VERSION, bpo::bool_switch(&version), "Print version of taraxad");
  main_options.add_options()(COMMAND, bpo::value<std::vector<std::string>>(&command)->multitoken(),
                             "Command arg:"
                             "\nnode                  Runs the actual node (default)"
                             "\nconfig       Only generate/overwrite config file with provided node command "
                             "option without starting the node"
                             "\naccount key           Generate new account or restore from a key (key is optional)"
                             "\nvrf key               Generate new VRF or restore from a key (key is optional)");
  node_command_options.add_options()(WALLET, bpo::value<std::string>(&wallet),
                                     "JSON wallet file (default: \"~/.taraxa/wallet.json\")");
  node_command_options.add_options()(CONFIG, bpo::value<std::string>(&config),
                                     "JSON configuration file (default: \"~/.taraxa/config.json\")");
  node_command_options.add_options()(GENESIS, bpo::value<std::string>(&genesis),
                                     "JSON genesis file (default: \"~/.taraxa/genesis.json\")");
  node_command_options.add_options()(DATA_DIR, bpo::value<std::string>(&data_dir),
                                     "Data directory for the databases, logs ... (default: \"~/.taraxa/data\")");
  node_command_options.add_options()(DESTROY_DB, bpo::bool_switch(&destroy_db),
                                     "Destroys all the existing data in the database");
  node_command_options.add_options()(REBUILD_DB, bpo::bool_switch(&rebuild_db),
                                     "Reads the raw dag/pbft blocks from the db "
                                     "and executes all the blocks from scratch "
                                     "rebuilding all the other "
                                     "database tables - this could take a long "
                                     "time");
  node_command_options.add_options()(REBUILD_DB_COLUMNS, bpo::bool_switch(&rebuild_db_columns),
                                     "Removes old DB columns ");
  node_command_options.add_options()(REBUILD_DB_PERIOD, bpo::value<uint64_t>(&rebuild_db_period),
                                     "Use with rebuild-db - Rebuild db up "
                                     "to a specified period");
  node_command_options.add_options()(REBUILD_NETWORK, bpo::bool_switch(&rebuild_network),
                                     "Delete all saved network/nodes information "
                                     "and rebuild network from boot nodes");
  node_command_options.add_options()(REVERT_TO_PERIOD, bpo::value<uint64_t>(&revert_to_period),
                                     "Revert db/state to specified "
                                     "period (specify period)");
  node_command_options.add_options()(LIGHT, bpo::bool_switch(&light_node), "Enable light node functionality");
  node_command_options.add_options()(CHAIN_ID, bpo::value<int>(&chain_id),
                                     "Chain identifier (integer, 841=Mainnet, 842=Testnet, 843=Devnet) (default: 841) "
                                     "Only used when creating new config file");
  node_command_options.add_options()(CHAIN, bpo::value<std::string>(&chain_str),
                                     "Chain identifier (string, mainnet, testnet, devnet) (default: mainnet) "
                                     "Only used when creating new config file");

  node_command_options.add_options()(BOOT_NODES, bpo::value<std::vector<std::string>>(&boot_nodes)->multitoken(),
                                     "Boot nodes to connect to: [ip_address:port_number/node_id, ....]");
  node_command_options.add_options()(
      BOOT_NODES_APPEND, bpo::value<std::vector<std::string>>(&boot_nodes_append)->multitoken(),
      "Boot nodes to connect to in addition to boot nodes defined in config: [ip_address:port_number/node_id, ....]");
  node_command_options.add_options()(PUBLIC_IP, bpo::value<std::string>(&public_ip),
                                     "Force advertised public IP to the given IP (default: auto)");
  node_command_options.add_options()(PORT, bpo::value<uint16_t>(&port),
                                     "Listen on the given port for incoming connections");
  node_command_options.add_options()(LOG_CHANNELS, bpo::value<std::vector<std::string>>(&log_channels)->multitoken(),
                                     "Log channels to log: [channel:level, ....]");
  node_command_options.add_options()(
      LOG_CHANNELS_APPEND, bpo::value<std::vector<std::string>>(&log_channels_append)->multitoken(),
      "Log channels to log in addition to log channels defined in config: [channel:level, ....]");
  node_command_options.add_options()(LOG_CONFIGURATIONS,
                                     bpo::value<std::vector<std::string>>(&log_configurations)->multitoken(),
                                     "Log confifugrations to use: [configuration_name, ....]");
  node_command_options.add_options()(NODE_SECRET, bpo::value<std::string>(&node_secret), "Nose secret key to use");

  node_command_options.add_options()(VRF_SECRET, bpo::value<std::string>(&vrf_secret), "Vrf secret key to use");

  node_command_options.add_options()(
      OVERWRITE_CONFIG, bpo::bool_switch(&overwrite_config),
      "Overwrite config - "
      "Options data-dir, boot-nodes, log-channels, node-secret and vrf-secret are always used in running a node but "
      "only written to config file if overwrite-config flag is set. \n"
      "WARNING: Overwrite-config set can override/delete current secret keys in the wallet");

  node_command_options.add_options()(ENABLE_TEST_RPC, bpo::bool_switch(&enable_test_rpc),
                                     "Enables Test JsonRPC. Disabled by default");
  node_command_options.add_options()(ENABLE_DEBUG, bpo::bool_switch(&enable_debug),
                                     "Enables Debug RPC interface. Disabled by default");

  allowed_options.add(main_options);

  allowed_options.add(node_command_options);
  bpo::variables_map option_vars;

  auto parsed_line = bpo::parse_command_line(argc, argv, allowed_options);

  bpo::store(parsed_line, option_vars);
  bpo::notify(option_vars);
  if (option_vars.count(HELP)) {
    std::cout << "NAME:\n  "
                 "taraxad - Taraxa blockchain full node implementation\n"
                 "VERSION:\n  "
              << TARAXA_VERSION << "\nUSAGE:\n  taraxad [options]\n";
    std::cout << main_options << std::endl;
    std::cout << node_command_options << std::endl;
    // If help message requested, ignore any additional commands
    command.clear();
    return;
  }
  if (version) {
    std::cout << kVersionJson << std::endl;
    // If version requested, ignore any additional commands
    command.clear();
    return;
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
    if (!chain_str.empty()) {
      if (chain_id != static_cast<int>(DEFAULT_CHAIN_ID)) {
        std::cout << "You can not specify both " << CHAIN_ID << " and " << CHAIN << std::endl;
        return;
      }
      chain_id = tools::getChainIdFromString(chain_str);
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
      auto default_genesis_json = tools::getGenesis((Config::ChainIdType)chain_id);
      // override hardforks data with one from default json
      addNewHardforks(genesis_json, default_genesis_json);
      // add vote_eligibility_balance_step field if it is missing in the config
      if (genesis_json["dpos"]["vote_eligibility_balance_step"].isNull()) {
        genesis_json["dpos"]["vote_eligibility_balance_step"] =
            default_genesis_json["dpos"]["vote_eligibility_balance_step"];
      }
      write_config_and_wallet_files();
    }
    // Override config values with values from CLI
    config_json = tools::overrideConfig(config_json, data_dir, boot_nodes, log_channels, log_configurations,
                                        boot_nodes_append, log_channels_append);
    wallet_json = tools::overrideWallet(wallet_json, node_secret, vrf_secret);

    if (light_node) {
      config_json["is_light_node"] = true;
    }

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

    if (destroy_db) {
      fs::remove_all(node_config_.db_path);
    }
    if (rebuild_network) {
      fs::remove_all(node_config_.net_file_path());
    }
    if (!public_ip.empty()) {
      node_config_.network.public_ip = public_ip;
    }
    if (port) {
      node_config_.network.listen_port = port;
    }
    node_config_.db_config.db_revert_to_period = revert_to_period;
    node_config_.db_config.rebuild_db = rebuild_db;
    node_config_.db_config.rebuild_db_columns = rebuild_db_columns;
    node_config_.db_config.rebuild_db_period = rebuild_db_period;

    node_config_.enable_test_rpc = enable_test_rpc;
    node_config_.enable_debug = enable_debug;

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

bool Config::nodeConfigured() { return node_configured_; }

FullNodeConfig Config::getNodeConfiguration() { return node_config_; }

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

std::string Config::dirNameFromFile(const string& file) {
  size_t pos = file.find_last_of("\\/");
  return (string::npos == pos) ? "" : file.substr(0, pos);
}

}  // namespace taraxa::cli
