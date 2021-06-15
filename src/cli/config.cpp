#include "cli/config.hpp"

#include <iostream>

#include "cli/tools.hpp"

using namespace std;

namespace bpo = boost::program_options;

namespace taraxa::cli {

Config::Config(int argc, const char* argv[], const string& taraxa_version) {
  boost::program_options::options_description main_options("OPTIONS");
  boost::program_options::options_description node_command_options("NODE COMMAND OPTIONS");
  boost::program_options::options_description allowed_options("Allowed options");
  string config;
  string wallet;
  int network_id = (int)DEFAULT_NETWORK_ID;
  string data_dir;
  vector<string> command;
  vector<string> boot_nodes;
  vector<string> log_channels;
  vector<string> boot_nodes_append;
  vector<string> log_channels_append;
  string node_secret;
  string vrf_secret;
  bool overwrite_config;

  bool boot_node = false;
  bool destroy_db = 0;
  bool rebuild_network = 0;
  bool rebuild_db = 0;
  bool version = false;
  uint64_t rebuild_db_period = 0;
  uint64_t revert_to_period = 0;
  // Set node as default command
  command.push_back(NODE_COMMAND);

  // Set config file and data directory to default values
  config = Tools::getTaraxaDefaultConfigFile();
  wallet = Tools::getWalletDefaultFile();
  auto default_dir = Tools::getTaraxaDefaultDir();

  // Define all the command line options and descriptions
  main_options.add_options()(HELP, "Print this help message and exit");
  main_options.add_options()(VERSION, bpo::bool_switch(&version), "Print version of taraxad");
  main_options.add_options()(COMMAND, bpo::value<vector<string>>(&command)->multitoken(),
                             "Command arg:"
                             "\nnode                  Runs the actual node (default)"
                             "\nconfig       Only generate/overwrite config file with provided node command "
                             "option without starting the node"
                             "\naccount key           Generate new account or restore from a key (key is optional)"
                             "\nvrf key               Generate new VRF or restore from a key (key is optional)");
  node_command_options.add_options()(WALLET, bpo::value<string>(&wallet),
                                     "JSON wallet file (default: \"~/.taraxa/wallet.json\")");
  node_command_options.add_options()(CONFIG, bpo::value<string>(&config),
                                     "JSON configuration file (default: \"~/.taraxa/config.json\")");
  node_command_options.add_options()(DATA_DIR, bpo::value<string>(&data_dir),
                                     "Data directory for the databases (default: \"~/.taraxa/db\")");
  node_command_options.add_options()(DESTROY_DB, bpo::bool_switch(&destroy_db),
                                     "Destroys all the existing data in the database");
  node_command_options.add_options()(REBUILD_DB, bpo::bool_switch(&rebuild_db),
                                     "Reads the raw dag/pbft blocks from the db "
                                     "and executes all the blocks from scratch "
                                     "rebuilding all the other "
                                     "database tables - this could take a long "
                                     "time");
  node_command_options.add_options()(REBUILD_DB_PERIOD, bpo::value<uint64_t>(&rebuild_db_period),
                                     "Use with rebuild-db - Rebuild db up "
                                     "to a specified period");
  node_command_options.add_options()(REBUILD_NETWORK, bpo::bool_switch(&rebuild_network),
                                     "Delete all saved network/nodes information "
                                     "and rebuild network from boot nodes");
  node_command_options.add_options()(REVERT_TO_PERIOD, bpo::value<uint64_t>(&revert_to_period),
                                     "Revert db/state to specified "
                                     "period (specify period)");
  node_command_options.add_options()(NETWORK_ID, bpo::value<int>(&network_id),
                                     "Network identifier (integer, 1=Mainnet, 2=Testnet, 3=Devnet) (default: 2)"
                                     "Only used when creating new config file");
  node_command_options.add_options()(BOOT_NODE, bpo::bool_switch(&boot_node), "Run as bootnode (default: false)");

  node_command_options.add_options()(BOOT_NODES, bpo::value<vector<string>>(&boot_nodes)->multitoken(),
                                     "Boot nodes to connect to: [ip_address:port_number/node_id, ....]");
  node_command_options.add_options()(
      BOOT_NODES_APPEND, bpo::value<vector<string>>(&boot_nodes_append)->multitoken(),
      "Boot nodes to connect to in addition to boot nodes defined in config: [ip_address:port_number/node_id, ....]");
  node_command_options.add_options()(LOG_CHANNELS, bpo::value<vector<string>>(&log_channels)->multitoken(),
                                     "Log channels to log: [channel:level, ....]");
  node_command_options.add_options()(
      LOG_CHANNELS_APPEND, bpo::value<vector<string>>(&log_channels_append)->multitoken(),
      "Log channels to log in addition to log channels defined in config: [channel:level, ....]");
  node_command_options.add_options()(NODE_SECRET, bpo::value<string>(&node_secret), "Nose secret key to use");

  node_command_options.add_options()(VRF_SECRET, bpo::value<string>(&vrf_secret), "Vrf secret key to use");

  node_command_options.add_options()(
      OVERWRITE_CONFIG, bpo::bool_switch(&overwrite_config),
      "Overwrite config - "
      "Options data-dir, boot-nodes, log-channels, node-secret and vrf-secret are always used in running a node but "
      "only written to config file if overwrite-config flag is set. \n"
      "WARNING: Overwrite-config set can override/delete current secret keys in the wallet");

  allowed_options.add(main_options);

  allowed_options.add(node_command_options);
  bpo::variables_map option_vars;

  auto parsed_line = bpo::parse_command_line(argc, argv, allowed_options);

  bpo::store(parsed_line, option_vars);
  bpo::notify(option_vars);
  if (option_vars.count(HELP)) {
    cout << "NAME:\n  "
            "taraxad - Taraxa blockchain full node implementation\n"
            "VERSION:\n  "
         << taraxa_version << "\nUSAGE:\n  taraxad [options]\n";
    cout << main_options << endl;
    cout << node_command_options << endl;
    // If help message requested, ignore any additional commands
    command.clear();
    return;
  }
  if (version) {
    cout << taraxa_version << endl;
    // If version requested, ignore any additional commands
    command.clear();
    return;
  }

  if (command.size() > 0 && (command[0] == NODE_COMMAND || command[0] == CONFIG_COMMAND)) {
    // Create dir if missing
    auto config_dir = dirNameFromFile(config);
    auto wallet_dir = dirNameFromFile(wallet);
    if (!data_dir.empty() && !fs::exists(data_dir)) {
      fs::create_directories(data_dir);
    }
    if (!config_dir.empty() && !fs::exists(config_dir)) {
      fs::create_directories(config_dir);
    }
    if (!wallet_dir.empty() && !fs::exists(wallet_dir)) {
      fs::create_directories(wallet_dir);
    }

    // If any of the config files are missing they are generated with default values
    if (!fs::exists(config)) {
      cout << "Configuration file does not exist at: " << config << ". New config file will be generated" << endl;
      Tools::generateConfig(config, (Config::NetworkIdType)network_id);
    }
    if (!fs::exists(wallet)) {
      cout << "Wallet file does not exist at: " << wallet << ". New wallet file will be generated" << endl;
      Tools::generateWallet(wallet);
    }

    Json::Value config_json = Tools::readJsonFromFile(config);
    Json::Value wallet_json = Tools::readJsonFromFile(wallet);

    // Override config values with values from CLI
    config_json = Tools::overrideConfig(config_json, data_dir, boot_node, boot_nodes, log_channels, boot_nodes_append,
                                        log_channels_append);
    wallet_json = Tools::overrideWallet(wallet_json, node_secret, vrf_secret);

    // Save changes permanently if overwrite_config option is set
    // or if running config command
    // This can overwrite secret keys in wallet
    if (overwrite_config || command[0] == CONFIG_COMMAND) {
      Tools::writeJsonToFile(config, config_json);
      Tools::writeJsonToFile(wallet, wallet_json);
    }

    // Load config
    node_config_ = FullNodeConfig(config_json, wallet_json);

    // Validate config values
    node_config_.validate();

    if (destroy_db) {
      fs::remove_all(node_config_.db_path);
    }
    if (rebuild_network) {
      fs::remove_all(node_config_.net_file_path());
    }
    node_config_.test_params.db_revert_to_period = revert_to_period;
    node_config_.test_params.rebuild_db = rebuild_db;
    node_config_.test_params.rebuild_db_period = rebuild_db_period;
    if (command[0] == NODE_COMMAND) node_configured_ = true;
  } else if (command.size() > 0 && command[0] == ACCOUNT_COMMAND) {
    if (command.size() == 1)
      Tools::generateAccount();
    else
      Tools::generateAccountFromKey(command[1]);
  } else if (command.size() > 0 && command[0] == VRF_COMMAND) {
    if (command.size() == 1)
      Tools::generateVrf();
    else
      Tools::generateVrfFromKey(command[1]);
  } else {
    throw bpo::invalid_option_value(command[0]);
  }
}

bool Config::nodeConfigured() { return node_configured_; }

FullNodeConfig Config::getNodeConfiguration() { return node_config_; }

string Config::dirNameFromFile(const string& file) {
  size_t pos = file.find_last_of("\\/");
  return (string::npos == pos) ? "" : file.substr(0, pos);
}

}  // namespace taraxa::cli
