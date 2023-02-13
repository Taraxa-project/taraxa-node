
#include "cli/tools.hpp"

#include <pwd.h>

#include <boost/algorithm/string.hpp>
#include <filesystem>

#include "cli/config_parser.hpp"
#include "common/jsoncpp.hpp"

using namespace std;
using namespace dev;
namespace fs = std::filesystem;

namespace taraxa::cli::tools {

int getChainIdFromString(std::string& chain_str) {
  boost::algorithm::to_lower(chain_str);
  if (chain_str == "mainnet") {
    return static_cast<int>(ConfigParser::ChainIdType::Mainnet);
  } else if (chain_str == "testnet") {
    return static_cast<int>(ConfigParser::ChainIdType::Testnet);
  } else if (chain_str == "devnet") {
    return static_cast<int>(ConfigParser::ChainIdType::Devnet);
  }
  throw boost::program_options::invalid_option_value(chain_str);
}

Json::Value getConfig(ConfigParser::ChainIdType chain_id) {
  Json::Value conf;
  switch (chain_id) {
    case ConfigParser::ChainIdType::Mainnet:
      conf = util::readJsonFromString(mainnet_config_json);
      break;
    case ConfigParser::ChainIdType::Testnet:
      conf = util::readJsonFromString(testnet_config_json);
      break;
    case ConfigParser::ChainIdType::Devnet:
      conf = util::readJsonFromString(devnet_config_json);
      break;
    default:
      conf = util::readJsonFromString(default_config_json);
  }
  return conf;
}

Json::Value getGenesis(ConfigParser::ChainIdType chain_id) {
  Json::Value genesis;
  switch (chain_id) {
    case ConfigParser::ChainIdType::Mainnet:
      genesis = util::readJsonFromString(mainnet_genesis_json);
      break;
    case ConfigParser::ChainIdType::Testnet:
      genesis = util::readJsonFromString(testnet_genesis_json);
      break;
    case ConfigParser::ChainIdType::Devnet:
      genesis = util::readJsonFromString(devnet_genesis_json);
      break;
    default:
      genesis = util::readJsonFromString(default_genesis_json);
      genesis["chain_id"] = static_cast<int>(chain_id);
  }
  return genesis;
}

void generateWallet(const string& wallet) {
  // Wallet
  dev::KeyPair account = dev::KeyPair::create();
  auto [pk, sk] = taraxa::vrf_wrapper::getVrfKeyPair();
  auto account_json = createWalletJson(account, sk, pk);

  // Create account file
  util::writeJsonToFile(wallet, account_json);
}

void generateAccount(const dev::KeyPair& account) {
  cout << "\"node_secret\" : \"" << toHex(account.secret().ref()) << "\"" << endl;
  cout << "\"node_public\" : \"" << account.pub().toString() << "\"" << endl;
  cout << "\"node_address\" : \"" << account.address().toString() << "\"" << endl;
}

void generateAccount() { generateAccount(dev::KeyPair::create()); }

void generateAccountFromKey(const string& key) {
  auto sk = dev::Secret(key, dev::Secret::ConstructFromStringType::FromHex);
  auto account = dev::KeyPair(sk);
  generateAccount(account);
}

void generateVrf(const taraxa::vrf_wrapper::vrf_sk_t& sk, const taraxa::vrf_wrapper::vrf_pk_t& pk) {
  cout << "\"vrf_secret\" : \"" << sk.toString() << "\"" << endl;
  cout << "\"vrf_public\" : \"" << pk.toString() << "\"" << endl;
}

void generateVrf() {
  auto [pk, sk] = taraxa::vrf_wrapper::getVrfKeyPair();
  generateVrf(sk, pk);
}

void generateVrfFromKey(const string& key) {
  auto sk = taraxa::vrf_wrapper::vrf_sk_t(key);
  auto pk = taraxa::vrf_wrapper::getVrfPublicKey(sk);
  generateVrf(sk, pk);
}

Json::Value createWalletJson(const dev::KeyPair& account, const taraxa::vrf_wrapper::vrf_sk_t& sk,
                             const taraxa::vrf_wrapper::vrf_pk_t& pk) {
  Json::Value json(Json::objectValue);
  json["node_secret"] = toHex(account.secret().ref());
  json["node_public"] = account.pub().toString();
  json["node_address"] = account.address().toString();
  json["vrf_secret"] = sk.toString();
  json["vrf_public"] = pk.toString();
  return json;
}

string getHomeDir() { return string(getpwuid(getuid())->pw_dir); }

string getTaraxaDefaultDir() { return getHomeDir() + "/" + DEFAULT_TARAXA_DIR_NAME; }

string getTaraxaDataDefaultDir() { return getHomeDir() + "/" + DEFAULT_TARAXA_DATA_DIR_NAME; }

string getTaraxaDefaultWalletFile() { return getTaraxaDefaultDir() + "/" + DEFAULT_WALLET_FILE_NAME; }

string getTaraxaDefaultConfigFile() { return getTaraxaDefaultDir() + "/" + DEFAULT_CONFIG_FILE_NAME; }

string getTaraxaDefaultGenesisFile() { return getTaraxaDefaultDir() + "/" + DEFAULT_GENESIS_FILE_NAME; }

}  // namespace taraxa::cli::tools
