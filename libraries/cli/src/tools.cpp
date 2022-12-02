
#include "cli/tools.hpp"

#include <pwd.h>

#include <boost/algorithm/string.hpp>
#include <filesystem>

#include "cli/config.hpp"
#include "common/jsoncpp.hpp"

using namespace std;
using namespace dev;
namespace fs = std::filesystem;

namespace taraxa::cli::tools {

int getChainIdFromString(std::string& chain_str) {
  boost::algorithm::to_lower(chain_str);
  if (chain_str == "mainnet") {
    return static_cast<int>(Config::ChainIdType::Mainnet);
  } else if (chain_str == "testnet") {
    return static_cast<int>(Config::ChainIdType::Testnet);
  } else if (chain_str == "devnet") {
    return static_cast<int>(Config::ChainIdType::Devnet);
  }
  throw boost::program_options::invalid_option_value(chain_str);
}

Json::Value getConfig(Config::ChainIdType chain_id) {
  Json::Value conf;
  switch (chain_id) {
    case Config::ChainIdType::Mainnet:
      conf = util::readJsonFromString(mainnet_config_json);
      break;
    case Config::ChainIdType::Testnet:
      conf = util::readJsonFromString(testnet_config_json);
      break;
    case Config::ChainIdType::Devnet:
      conf = util::readJsonFromString(devnet_config_json);
      break;
    default:
      conf = util::readJsonFromString(default_config_json);
      conf["chain_id"] = static_cast<int>(chain_id);
  }
  return conf;
}

Json::Value getGenesis(Config::ChainIdType chain_id) {
  Json::Value conf;
  switch (chain_id) {
    case Config::ChainIdType::Mainnet:
      conf = util::readJsonFromString(mainnet_genesis_json);
      break;
    case Config::ChainIdType::Testnet:
      conf = util::readJsonFromString(testnet_genesis_json);
      break;
    case Config::ChainIdType::Devnet:
      conf = util::readJsonFromString(devnet_genesis_json);
      break;
    default:
      conf = util::readJsonFromString(default_genesis_json);
  }
  return conf;
}

Json::Value overrideConfig(Json::Value& conf, std::string& data_dir, vector<string> boot_nodes,
                           vector<string> log_channels, vector<string> log_configurations,
                           const vector<string>& boot_nodes_append, const vector<string>& log_channels_append) {
  if (data_dir.empty()) {
    if (conf["data_path"].asString().empty()) {
      conf["data_path"] = getTaraxaDataDefaultDir();
    }
    data_dir = conf["data_path"].asString();
  } else {
    conf["data_path"] = data_dir;
  }

  if (log_channels.size() > 0 && log_channels_append.size() > 0) {
    throw invalid_argument("log_channels and log_channels_append args are not allowed to be used together");
  }
  if (boot_nodes.size() > 0 && boot_nodes_append.size() > 0) {
    throw invalid_argument("boot_nodes and boot_nodes_append args are not allowed to be used together");
  }

  // Override boot nodes
  if (boot_nodes.size() > 0) {
    conf["network"]["boot_nodes"] = Json::Value(Json::arrayValue);
  }
  if (boot_nodes_append.size() > 0) {
    boot_nodes = boot_nodes_append;
  }
  if (boot_nodes.size() > 0) {
    for (auto const& b : boot_nodes) {
      vector<string> result;
      boost::split(result, b, boost::is_any_of(":/"));
      if (result.size() != 3) throw invalid_argument("Boot node in boot_nodes not specified correctly");
      bool found = false;
      for (Json::Value& node : conf["network"]["boot_nodes"]) {
        if (node["id"].asString() == result[2]) {
          found = true;
          node["ip"] = result[0];
          node["listen_port"] = stoi(result[1]);
          node["port"] = stoi(result[1]);
        }
      }
      if (!found) {
        Json::Value b_node;
        b_node["id"] = result[2];
        b_node["ip"] = result[0];
        // b_node["listen_port"] = stoi(result[1]);
        b_node["port"] = stoi(result[1]);
        conf["network"]["boot_nodes"].append(b_node);
      }
    }
  }

  // Override log channels
  if (log_channels.size() > 0) {
    conf["logging"]["configurations"][0u]["channels"] = Json::Value(Json::arrayValue);
  }

  // Turn on logging configurations
  if (log_configurations.size() > 0) {
    for (Json::Value& node : conf["logging"]["configurations"]) {
      for (const auto& log_conf : log_configurations) {
        if (node["name"].asString() == log_conf) {
          node["on"] = true;
        }
      }
    }
  }
  if (log_channels_append.size() > 0) {
    log_channels = log_channels_append;
  }
  if (log_channels.size() > 0) {
    for (auto const& l : log_channels) {
      vector<string> result;
      boost::split(result, l, boost::is_any_of(":"));
      if (result.size() != 2) throw invalid_argument("Log channel in log_channels not specified correctly");

      bool found = false;
      for (Json::Value& node : conf["logging"]["configurations"][0u]["channels"]) {
        if (node["name"].asString() == result[0]) {
          found = true;
          node["verbosity"] = result[1];
        }
      }
      if (!found) {
        Json::Value channel_node;
        channel_node["name"] = result[0];
        channel_node["verbosity"] = result[1];
        conf["logging"]["configurations"][0u]["channels"].append(channel_node);
      }
    }
  }
  return conf;
}

void generateWallet(const string& wallet) {
  // Wallet
  dev::KeyPair account = dev::KeyPair::create();

  auto [pk, sk] = taraxa::vrf_wrapper::getVrfKeyPair();

  auto account_json = createWalletJson(account, sk, pk);

  // Create account file
  util::writeJsonToFile(wallet, account_json);
}

Json::Value overrideWallet(Json::Value& wallet, const string& node_key, const string& vrf_key) {
  if (!node_key.empty()) {
    auto sk = dev::Secret(node_key, dev::Secret::ConstructFromStringType::FromHex);
    dev::KeyPair account = dev::KeyPair(sk);
    wallet["node_secret"] = toHex(account.secret().ref());
    wallet["node_public"] = account.pub().toString();
    wallet["node_address"] = account.address().toString();
  }

  if (!vrf_key.empty()) {
    auto sk = taraxa::vrf_wrapper::vrf_sk_t(vrf_key);
    auto pk = taraxa::vrf_wrapper::getVrfPublicKey(sk);
    wallet["vrf_secret"] = sk.toString();
    wallet["vrf_public"] = pk.toString();
  }

  return wallet;
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
string getTaraxaDefaultGenesisFile() { return getTaraxaDefaultDir() + "/" + DEFAULT_CONFIG_FILE_NAME; }

}  // namespace taraxa::cli::tools
