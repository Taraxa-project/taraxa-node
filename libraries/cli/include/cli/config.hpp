#pragma once

#include <boost/program_options.hpp>
#include <string>

#include "config/config.hpp"

namespace taraxa::cli {

class Config {
 public:
  Config(int argc, const char* argv[]);

  ~Config() {}

  // Returns true if node configuration is loaded successfully and command is node
  bool nodeConfigured();

  // Retrieves loaded node configuration
  FullNodeConfig getNodeConfiguration();

  enum class NetworkIdType { Mainnet = 1, Testnet, Devnet };
  static constexpr NetworkIdType DEFAULT_NETWORK_ID = NetworkIdType::Testnet;

 protected:
  FullNodeConfig node_config_;
  bool node_configured_ = false;

  static constexpr const char* CONFIG = "config";
  static constexpr const char* DATA_DIR = "data-dir";
  static constexpr const char* NETWORK_ID = "network-id";
  static constexpr const char* BOOT_NODE = "boot-node";
  static constexpr const char* COMMAND = "command";
  static constexpr const char* DESTROY_DB = "destroy-db";
  static constexpr const char* REBUILD_NETWORK = "rebuild-network";
  static constexpr const char* REBUILD_DB = "rebuild-db";
  static constexpr const char* REBUILD_DB_PERIOD = "rebuild-db-period";
  static constexpr const char* REVERT_TO_PERIOD = "revert-to-period";
  static constexpr const char* REBUILD_DB_COLUMNS = "rebuild-db-columns";
  static constexpr const char* HELP = "help";
  static constexpr const char* VERSION = "version";
  static constexpr const char* WALLET = "wallet";
  static constexpr const char* SET_PBFT_STEP = "set-pbft-step";

  static constexpr const char* NODE_COMMAND = "node";
  static constexpr const char* ACCOUNT_COMMAND = "account";
  static constexpr const char* VRF_COMMAND = "vrf";
  static constexpr const char* CONFIG_COMMAND = "config";
  static constexpr const char* BOOT_NODES = "boot-nodes";
  static constexpr const char* LOG_CHANNELS = "log-channels";
  static constexpr const char* BOOT_NODES_APPEND = "boot-nodes-append";
  static constexpr const char* LOG_CHANNELS_APPEND = "log-channels-append";
  static constexpr const char* NODE_SECRET = "node-secret";
  static constexpr const char* VRF_SECRET = "vrf-secret";
  static constexpr const char* OVERWRITE_CONFIG = "overwrite-config";

  std::string dirNameFromFile(const std::string& file);
};

}  // namespace taraxa::cli