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
  // Variables for parsed command line options
  int chain_id = static_cast<int>(kDefaultChainId);

  std::string genesis, config, wallet, chain_str, data_dir, node_secret, vrf_secret;
  // Set config file and data directory to default values
  config = tools::getTaraxaDefaultConfigFile();
  wallet = tools::getTaraxaDefaultWalletFile();
  genesis = tools::getTaraxaDefaultGenesisFile();

  std::vector<std::string> command, boot_nodes, log_channels, log_configurations, boot_nodes_append,
      log_channels_append;
  // Set node as default command
  command.push_back(kNodeCommand);

  bool overwrite_config, destroy_db, rebuild_network, light_node;
  overwrite_config = destroy_db = rebuild_network = light_node = false;

  // Define all the command line options and descriptions
  boost::program_options::options_description node_cli_options("Node command line options");
  // Lambda for adding command line options
  auto addCliOption = node_cli_options.add_options();

  // TODO: these will be replaced by config option
  addCliOption("data-dir", bpo::value<std::string>(&data_dir),
               "Data directory for the databases, logs ... (default: \"~/.taraxa/data\")");
  addCliOption("chain-id", bpo::value<int>(&chain_id),
               "Chain identifier (integer, 841=Mainnet, 842=Testnet, 843=Devnet) (default: 841) "
               "Only used when creating new config file");
  addCliOption("light", bpo::bool_switch(&light_node), "Enable light node functionality");
  addCliOption("boot-nodes", bpo::value<std::vector<std::string>>(&boot_nodes)->multitoken(),
               "Boot nodes to connect to: [ip_address:port_number/node_id, ....]");
  addCliOption("port", bpo::value<uint16_t>(&node_config_.network.listen_port),
               "Listen on the given port for incoming connections");
  addCliOption("log-channels", bpo::value<std::vector<std::string>>(&log_channels)->multitoken(),
               "Log channels to log: [channel:level, ....]");
  addCliOption("log-configurations", bpo::value<std::vector<std::string>>(&log_configurations)->multitoken(),
               "Log confifugrations to use: [configuration_name, ....]");
  addCliOption(
      "boot-nodes-append", bpo::value<std::vector<std::string>>(&boot_nodes_append)->multitoken(),
      "Boot nodes to connect to in addition to boot nodes defined in config: [ip_address:port_number/node_id, ....]");
  addCliOption("log-channels-append", bpo::value<std::vector<std::string>>(&log_channels_append)->multitoken(),
               "Log channels to log in addition to log channels defined in config: [channel:level, ....]");
  // TODO: what do we need overwrite-config for ?
  addCliOption(
      "overwrite-config", bpo::bool_switch(&overwrite_config),
      "Overwrite config - "
      "Options data-dir, boot-nodes, log-channels, node-secret and vrf-secret are always used in running a node but "
      "only written to config file if overwrite-config flag is set. \n"
      "WARNING: Overwrite-config set can override/delete current secret keys in the wallet");
  // TODO: end -> these will be replaced by config option

  addCliOption("help", "Print this help message and exit");
  addCliOption("version", bpo::bool_switch(), "Print version of taraxad");
  addCliOption("command", bpo::value<std::vector<std::string>>(&command)->multitoken(),
               "Command arg:"
               "\nnode                  Runs the actual node (default)"
               "\nconfig       Only generate/overwrite config file with provided node command "
               "option without starting the node"
               "\naccount key           Generate new account or restore from a key (key is optional)"
               "\nvrf key               Generate new VRF or restore from a key (key is optional)");
  addCliOption("wallet", bpo::value<std::string>(&wallet), "JSON wallet file (default: \"~/.taraxa/wallet.json\")");
  addCliOption("config", bpo::value<std::string>(&config),
               "JSON configuration file (default: \"~/.taraxa/config.json\")");
  addCliOption("genesis", bpo::value<std::string>(&genesis), "JSON genesis file (default: \"~/.taraxa/genesis.json\")");
  addCliOption("destroy-db", bpo::bool_switch(&destroy_db), "Destroys all the existing data in the database");
  addCliOption("rebuild-db", bpo::bool_switch(&node_config_.db_config.rebuild_db),
               "Reads the raw dag/pbft blocks from the db "
               "and executes all the blocks from scratch "
               "rebuilding all the other "
               "database tables - this could take a long "
               "time");
  addCliOption("rebuild-db-columns", bpo::bool_switch(&node_config_.db_config.rebuild_db_columns),
               "Removes old DB columns ");
  addCliOption("rebuild-db-period", bpo::value<uint64_t>(&node_config_.db_config.rebuild_db_period),
               "Use with rebuild-db - Rebuild db up "
               "to a specified period");
  addCliOption("rebuild-network", bpo::bool_switch(&rebuild_network),
               "Delete all saved network/nodes information "
               "and rebuild network from boot nodes");
  addCliOption("revert-to-period", bpo::value<uint64_t>(&node_config_.db_config.db_revert_to_period),
               "Revert db/state to specified "
               "period (specify period)");
  addCliOption("chain", bpo::value<std::string>(&chain_str),
               "Chain identifier (string, mainnet, testnet, devnet) (default: mainnet) "
               "Only used when creating new config file");
  addCliOption("public-ip", bpo::value<std::string>(&node_config_.network.public_ip),
               "Force advertised public IP to the given IP (default: auto)");
  addCliOption("node-secret", bpo::value<std::string>(&node_secret), "Nose secret key to use");
  addCliOption("vrf-secret", bpo::value<std::string>(&vrf_secret), "Vrf secret key to use");
  addCliOption("enable-test-rpc", bpo::bool_switch(&node_config_.enable_test_rpc),
               "Enables Test JsonRPC. Disabled by default");
  addCliOption("debug", bpo::bool_switch(&node_config_.enable_debug), "Enables Debug RPC interface. Disabled by default");

  // Parse all options (command line + config)
  boost::program_options::options_description allowed_options("Allowed options");
  allowed_options.add(node_cli_options);
  bpo::variables_map option_vars;

  auto parsed_line = bpo::parse_command_line(argc, argv, allowed_options);

  bpo::store(parsed_line, option_vars);
  bpo::notify(option_vars);

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
      if (chain_id != static_cast<int>(kDefaultChainId)) {
        std::cout << "You can not specify both \"chain-d\" and \"chain\"" << std::endl;
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
    // TODO: this cli option seems useless ?
    if (!genesis_json.isNull()) {
      auto tmp_genesis_json = tools::getGenesis((Config::ChainIdType)chain_id);
      // override hardforks data with one from default json
      addNewHardforks(genesis_json, tmp_genesis_json);
      // add vote_eligibility_balance_step field if it is missing in the config
      if (genesis_json["dpos"]["vote_eligibility_balance_step"].isNull()) {
        genesis_json["dpos"]["vote_eligibility_balance_step"] =
            tmp_genesis_json["dpos"]["vote_eligibility_balance_step"];
      }
      write_config_and_wallet_files();
    }
    // Override config values with values from CLI
    config_json = tools::overrideConfig(config_json, data_dir, boot_nodes, log_channels, log_configurations,
                                        boot_nodes_append, log_channels_append);
    wallet_json = tools::overrideWallet(wallet_json, node_secret, vrf_secret);

    // TODO: will be replaced by config option
    if (light_node) {
      config_json["is_light_node"] = true;
    }

    // Create data directory
    // TODO: will be replaced by config option
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
    if (overwrite_config || command[0] == kConfigCommand) {
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

std::string Config::dirNameFromFile(const string& file) const {
  size_t pos = file.find_last_of("\\/");
  return (string::npos == pos) ? "" : file.substr(0, pos);
}

}  // namespace taraxa::cli
