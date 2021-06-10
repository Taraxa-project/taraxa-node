#include "cli/config.hpp"

#include <iostream>

#include "cli/tools.hpp"

using namespace std;

namespace bpo = boost::program_options;

namespace taraxa::cli {

Config::Config(int argc, const char* argv[], const std::string& taraxa_version) {
  boost::program_options::options_description main_options("OPTIONS");
  boost::program_options::options_description node_command_options("NODE COMMAND OPTIONS");
  boost::program_options::options_description allowed_options("Allowed options");

  std::string config;
  std::string wallet;
  int network_id = (int)DEFAULT_NETWORK_ID;
  std::string data_dir;
  std::vector<std::string> command;
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
  data_dir = Tools::getTaraxaDataDefaultDir();
  wallet = Tools::getWalletDefaultFile();
  auto default_dir = Tools::getTaraxaDefaultDir();

  // Define all the command line options and descriptions
  main_options.add_options()(HELP, "Print this help message and exit");
  main_options.add_options()(VERSION, bpo::bool_switch(&version), "Print version of taraxad");
  main_options.add_options()(COMMAND, bpo::value<vector<std::string>>(&command)->multitoken(),
                             "Command arg:"
                             "\nnode                  Runs the actual node (default)"
                             "\naccount               Generate new account private and public key"
                             "\naccount-from-key      Restore account from private key"
                             "\nvrf                   Generate new VRF private and public key"
                             "\nvrf-from-key arg      Restore VRF from private key");
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
                                     "Network identifier (integer, 1=Mainnet, 2=Testnet, 3=Devnet) (default: 2)");
  node_command_options.add_options()(BOOT_NODE, bpo::bool_switch(&boot_node), "Run as bootnode (default: false)");

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

  if (command.size() > 0 && command[0] == NODE_COMMAND) {
    // If any of the config files are missing they are generated with default values
    if (!fs::exists(config)) {
      cout << "Configuration file does not exist at: " << config << ". New config file will be generated" << endl;
      Tools::generateConfig(config, data_dir, boot_node, (Config::NetworkIdType)network_id);
    }
    if (!fs::exists(wallet)) {
      cout << "Wallet file does not exist at: " << wallet << ". New wallet file will be generated" << endl;
      Tools::generateWallet(wallet);
    }

    // Load config
    node_config_ = FullNodeConfig(config, Tools::readJsonFromFile(wallet));

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
    node_configured_ = true;
  } else if (command.size() > 0 && command[0] == ACCOUNT_COMMAND) {
    Tools::generateAccount();
  } else if (command.size() > 1 && command[0] == ACCOUNT_FROM_KEY_COMMAND) {
    Tools::generateAccountFromKey(command[1]);
  } else if (command.size() > 0 && command[0] == VRF_COMMAND) {
    Tools::generateVrf();
  } else if (command.size() > 1 && command[0] == VRF_FROM_KEY_COMMAND) {
    Tools::generateVrfFromKey(command[1]);
  } else {
    throw bpo::invalid_option_value(command[0]);
  }
}

bool Config::nodeConfigured() { return node_configured_; }

FullNodeConfig Config::getNodeConfiguration() { return node_config_; }

std::string Config::dirNameFromFile(const std::string& file) {
  size_t pos = file.find_last_of("\\/");
  return (std::string::npos == pos) ? "" : file.substr(0, pos);
}

}  // namespace taraxa::cli
