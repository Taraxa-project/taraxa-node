#pragma once

#include <boost/program_options.hpp>
#include <string>

#include "cli/configs.hpp"
#include "config/config.hpp"

namespace taraxa::cli {

class Config {
 public:
  Config(int argc, const char* argv[]);

  // Returns true if node configuration is loaded successfully and command is node
  bool nodeConfigured();

  // Retrieves loaded node configuration
  FullNodeConfig getNodeConfiguration();

  static void addNewHardforks(Json::Value& config, const Json::Value& default_config);

  enum class ChainIdType { Mainnet = 841, Testnet, Devnet, LastNetworkId };
  static constexpr ChainIdType DEFAULT_CHAIN_ID = ChainIdType::Mainnet;

 protected:
  FullNodeConfig node_config_;
  bool node_configured_ = false;

  static constexpr const char* CONFIG = "config";
  static constexpr const char* GENESIS = "genesis";
  static constexpr const char* DATA_DIR = "data-dir";
  static constexpr const char* CHAIN_ID = "chain-id";
  static constexpr const char* CHAIN = "chain";
  static constexpr const char* COMMAND = "command";
  static constexpr const char* DESTROY_DB = "destroy-db";
  static constexpr const char* REBUILD_NETWORK = "rebuild-network";
  static constexpr const char* REBUILD_DB = "rebuild-db";
  static constexpr const char* REBUILD_DB_PERIOD = "rebuild-db-period";
  static constexpr const char* REVERT_TO_PERIOD = "revert-to-period";
  static constexpr const char* LIGHT = "light";
  static constexpr const char* HELP = "help";
  static constexpr const char* VERSION = "version";
  static constexpr const char* WALLET = "wallet";
  static constexpr const char* PRUNE_STATE_DB = "prune-state-db";

  static constexpr const char* NODE_COMMAND = "node";
  static constexpr const char* ACCOUNT_COMMAND = "account";
  static constexpr const char* VRF_COMMAND = "vrf";
  static constexpr const char* CONFIG_COMMAND = "config";
  static constexpr const char* BOOT_NODES = "boot-nodes";
  static constexpr const char* PUBLIC_IP = "public-ip";
  static constexpr const char* PORT = "port";
  static constexpr const char* LOG_CHANNELS = "log-channels";
  static constexpr const char* LOG_CONFIGURATIONS = "log-configurations";
  static constexpr const char* BOOT_NODES_APPEND = "boot-nodes-append";
  static constexpr const char* LOG_CHANNELS_APPEND = "log-channels-append";
  static constexpr const char* NODE_SECRET = "node-secret";
  static constexpr const char* VRF_SECRET = "vrf-secret";
  static constexpr const char* OVERWRITE_CONFIG = "overwrite-config";
  static constexpr const char* ENABLE_TEST_RPC = "enable-test-rpc";
  static constexpr const char* ENABLE_DEBUG = "debug";

  std::string dirNameFromFile(const std::string& file);
};

}  // namespace taraxa::cli