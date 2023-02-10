#pragma once

#include <boost/program_options.hpp>
#include <string>

#include "cli/configs.hpp"
#include "config/config.hpp"

namespace taraxa::cli {

class Config {
 public:
  enum class ChainIdType { Mainnet = 841, Testnet, Devnet, LastNetworkId };

  static constexpr ChainIdType kDefaultChainId = ChainIdType::Mainnet;

 public:
  Config(int argc, const char* argv[]);

  // Returns true if node configuration is loaded successfully and command is node
  bool nodeConfigured() const;

  // Retrieves loaded node configuration
  FullNodeConfig getNodeConfiguration() const;

  static void addNewHardforks(Json::Value& config, const Json::Value& default_config);

  FullNodeConfig createFullNodeConfig(const boost::program_options::options_description& cli_options,
                                      boost::program_options::variables_map& option_vars,
                                      const std::string config_path,
                                      std::optional<std::string> genesis_path = {},
                                      std::optional<std::string> wallet_path = {});

 private:
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
  void addCliOnlyOptionsToConfig(FullNodeConfig& config,
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
  void addUnregisteredConfigOptionsValues(FullNodeConfig& config, const std::string& config_path,
                                          const std::optional<std::string>& genesis_path,
                                          const std::optional<std::string>& wallet_path);

 private:
  FullNodeConfig node_config_;
  bool node_configured_ = false;
};

}  // namespace taraxa::cli
