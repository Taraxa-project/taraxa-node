#pragma once

#include <json/json.h>
#include <libdevcrypto/Common.h>

#include <string>

#include "cli/config_parser.hpp"
#include "common/vrf_wrapper.hpp"

namespace taraxa::cli::tools {

constexpr const char* DEFAULT_TARAXA_DIR_NAME = ".taraxa";
constexpr const char* DEFAULT_TARAXA_DATA_DIR_NAME = ".taraxa/data";
constexpr const char* DEFAULT_WALLET_FILE_NAME = "wallet.json";
constexpr const char* DEFAULT_CONFIG_FILE_NAME = "config.json";
constexpr const char* DEFAULT_GENESIS_FILE_NAME = "genesis.json";
const std::filesystem::path configs_path = std::filesystem::current_path() / "config_jsons";

// Account generation
void generateAccount();
void generateAccount(const dev::KeyPair& account);
void generateAccountFromKey(const std::string& key);

// VRF generation
void generateVrf();
void generateVrf(const taraxa::vrf_wrapper::vrf_sk_t& sk, const taraxa::vrf_wrapper::vrf_pk_t& pk);
void generateVrfFromKey(const std::string& key);

// Generate default config and wallet files
int getChainIdFromString(std::string& chain_str);
void getConfig(const std::string& config, cli::ConfigParser::ChainIdType chain_id);
Json::Value getConfig(ConfigParser::ChainIdType chain_id);
Json::Value getGenesis(ConfigParser::ChainIdType chain_id);
void generateWallet(const std::string& wallet);

std::string getHomeDir();
std::string getTaraxaDefaultDir();
std::string getTaraxaDataDefaultDir();
std::string getTaraxaDefaultConfigFile();
std::string getTaraxaDefaultWalletFile();
std::string getTaraxaDefaultGenesisFile();
Json::Value createWalletJson(const dev::KeyPair& account, const taraxa::vrf_wrapper::vrf_sk_t& sk,
                             const taraxa::vrf_wrapper::vrf_pk_t& pk);

}  // namespace taraxa::cli::tools