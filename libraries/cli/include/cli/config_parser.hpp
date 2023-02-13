#pragma once

#include <boost/program_options.hpp>
#include <string>

#include "cli/configs.hpp"
#include "config/config.hpp"

namespace taraxa::cli {

class ConfigParser {
 public:
  enum class ChainIdType { Mainnet = 841, Testnet, Devnet, LastNetworkId };
  static constexpr ChainIdType kDefaultChainId = ChainIdType::Mainnet;

 public:
  /**
   * @brief Creates FullNodeConfig based on provided command line arguments
   *
   * @return empty optional in case created config is invalid or specified command did not require config creation,
   *         otherwise newly created and parsed config
   */
  std::optional<FullNodeConfig> createConfig(int argc, const char* argv[]);

  bool updateConfigFromJson(FullNodeConfig& config, const std::string& json_config_path);

 private:
  /**
   * @brief Registeres options that can be specified only through command line
   *
   * @return options_description
   */
  boost::program_options::options_description registerCliOnlyOptions();

  /**
   * @brief Registeres options that can be specified in config as well as command line arguments
   *
   * @param config object into which are options directly parsed later on
   * @return options_description
   */
  boost::program_options::options_description registerCliConfigOptions(FullNodeConfig& config);

  /**
   * @brief Updates config based on json config values stored in file (json_config_path)
   *
   * @param config
   * @param json_config_path
   * @param genesis_path
   * @param wallet_path
   * @param cli_options
   */
  bool parseJsonConfigFiles(const boost::program_options::options_description& allowed_options,
                            boost::program_options::variables_map& option_vars, FullNodeConfig& config,
                            const std::string& json_config_path, std::optional<std::string> genesis_path = {},
                            std::optional<std::string> wallet_path = {});

  /**
   * @brief Adds command line arguments values(those that are not also part of config arguments) to the config stucture
   *
   * @param config
   * @param override_boot_nodes
   * @param append_boot_nodes
   * @param enable_log_configurations
   * @param override_log_channels
   * @param log_channels_append
   */
  static void updateConfigFromCliSpecialOptions(FullNodeConfig& config,
                                                const std::vector<std::string>& override_boot_nodes,
                                                const std::vector<std::string>& append_boot_nodes,
                                                const std::vector<std::string>& enable_log_configurations,
                                                const std::vector<std::string>& override_log_channels,
                                                const std::vector<std::string>& append_log_channels);

  /**
   * @brief Adds config values that are not registered as config cmd options
   *
   * @param config
   * @param config_path
   * @param genesis_path
   * @param wallet_path
   */
  static void parseUnregisteredConfigOptionsValues(FullNodeConfig& config, const std::string& config_path,
                                                   const std::optional<std::string>& genesis_path,
                                                   const std::optional<std::string>& wallet_path);

  /**
   * @brief Generates default paths and configs in case provided paths are either empty or they dont exist
   *
   * @param chain_id
   * @param config_path
   * @param genesis_path
   * @param wallet_path
   */
  void generateDefaultPathsAndConfigs(ConfigParser::ChainIdType chain_id, std::string& config_path,
                                      std::string& genesis_path, std::string& wallet_path);

 private:
  // Cli or config options that cannot be parsed directly to the FullNodeConfig object due to some required additional
  // processing or incompatible types
  std::string genesis_path_, config_path_, wallet_path_, chain_str_, node_secret_, vrf_secret_;
  std::vector<std::string> command_, boot_nodes_, log_channels_, log_configurations_, boot_nodes_append_,
      log_channels_append_;

  int chain_id_{static_cast<int>(kDefaultChainId)};
  bool overwrite_config_{false};
  bool destroy_db_{false};
  bool rebuild_network_{false};

  std::string data_path_str_;
  uint64_t packets_stats_time_period_ms_{0};
  uint64_t peer_max_packets_processing_time_us_{0};
};

}  // namespace taraxa::cli