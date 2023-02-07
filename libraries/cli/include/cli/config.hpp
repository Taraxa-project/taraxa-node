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

 protected:
  static constexpr const char* kNodeCommand = "node";
  static constexpr const char* kAccountCommand = "account";
  static constexpr const char* kVrfCommand = "vrf";
  static constexpr const char* kConfigCommand = "config";

  std::string dirNameFromFile(const std::string& file) const;

  FullNodeConfig node_config_;
  bool node_configured_ = false;
};

}  // namespace taraxa::cli
