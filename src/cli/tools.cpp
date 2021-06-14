#include "tools.hpp"

#include <pwd.h>

#include <boost/algorithm/string.hpp>
#include <filesystem>

#include "cli/config.hpp"
#include "default_config.hpp"
#include "devnet_config.hpp"
#include "testnet_config.hpp"

using namespace std;
using namespace dev;
namespace fs = std::filesystem;

namespace taraxa::cli {

void Tools::generateConfig(const std::string& config, Config::NetworkIdType network_id) {
  Json::Value conf;
  switch (network_id) {
    case Config::NetworkIdType::Testnet:
      conf = readJsonFromString(testnet_json);
      break;
    case Config::NetworkIdType::Devnet:
      conf = readJsonFromString(devnet_json);
      break;
    default:
      conf = readJsonFromString(default_json);
      conf["chain_config"]["chain_id"] = to_string((int)network_id);
  }
  writeJsonToFile(config, conf);
}

Json::Value Tools::overrideConfig(Json::Value& conf, const std::string& data_dir, bool boot_node,
                                  const vector<string>& boot_nodes, const vector<string>& log_channels,
                                  const string& override_config) {
  if (data_dir.empty()) {
    conf["db_path"] = getTaraxaDataDefaultDir();
  } else {
    conf["db_path"] = data_dir;
  }

  conf["network_is_boot_node"] = boot_node;
  
  //Override boot nodes
  if (boot_nodes.size() > 0) {
    if (override_config == Config::OVERRIDE_CONFIG_TRUNCATE) {
      conf["network_boot_nodes"] = Json::Value(Json::arrayValue);
    }
    for (auto const& b : boot_nodes) {
      vector<string> result;
      boost::split(result, b, boost::is_any_of(":/"));
      if (result.size() != 3) throw invalid_argument("Boot node in boot_nodes not specified correctly");
      bool found = false;
      for (Json::Value& node : conf["network_boot_nodes"]) {
        if (node["id"].asString() == result[2]) {
          found = true;
          node["ip"] = result[0];
          node["tcp_port"] = stoi(result[1]);
          node["udp_port"] = stoi(result[1]);
        }
      }
      if (!found) {
        Json::Value b_node;
        b_node["id"] = result[2];
        b_node["ip"] = result[0];
        b_node["tcp_port"] = stoi(result[1]);
        b_node["udp_port"] = stoi(result[1]);
        conf["network_boot_nodes"].append(b_node);
      }
    }
  }

  //Override log channels
  if (log_channels.size() > 0) {
    if (override_config == Config::OVERRIDE_CONFIG_TRUNCATE) {
      conf["logging"]["configurations"][0u]["channels"] = Json::Value(Json::arrayValue);
    }
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

void Tools::generateWallet(const string& wallet) {
  // Wallet
  dev::KeyPair account = dev::KeyPair::create();

  auto [pk, sk] = taraxa::vrf_wrapper::getVrfKeyPair();

  auto account_json = createWalletJson(account, sk, pk);

  // Create account file
  writeJsonToFile(wallet, account_json);
}

Json::Value Tools::overrideWallet(Json::Value& wallet, const string& node_key, const string& vrf_key) {
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

void Tools::generateAccount(const dev::KeyPair& account) {
  cout << "secret : " << toHex(account.secret().ref()) << endl;
  cout << "public : " << account.pub().toString() << endl;
  cout << "address : " << account.address().toString() << endl;
}

void Tools::generateAccount() { generateAccount(dev::KeyPair::create()); }

void Tools::generateAccountFromKey(const string& key) {
  auto sk = dev::Secret(key, dev::Secret::ConstructFromStringType::FromHex);
  auto account = dev::KeyPair(sk);
  generateAccount(account);
}

void Tools::generateVrf(const taraxa::vrf_wrapper::vrf_sk_t& sk, const taraxa::vrf_wrapper::vrf_pk_t& pk) {
  cout << "vrf_secret : " << sk.toString() << endl;
  cout << "vrf_public : " << pk.toString() << endl;
}

void Tools::generateVrf() {
  auto [pk, sk] = taraxa::vrf_wrapper::getVrfKeyPair();
  generateVrf(sk, pk);
}

void Tools::generateVrfFromKey(const string& key) {
  auto sk = taraxa::vrf_wrapper::vrf_sk_t(key);
  auto pk = taraxa::vrf_wrapper::getVrfPublicKey(sk);
  generateVrf(sk, pk);
}

Json::Value Tools::createWalletJson(const dev::KeyPair& account, const taraxa::vrf_wrapper::vrf_sk_t& sk,
                                    const taraxa::vrf_wrapper::vrf_pk_t& pk) {
  Json::Value json(Json::objectValue);
  json["node_secret"] = toHex(account.secret().ref());
  json["node_public"] = account.pub().toString();
  json["node_address"] = account.address().toString();
  json["vrf_secret"] = sk.toString();
  json["vrf_public"] = pk.toString();
  return json;
}

string Tools::getHomeDir() { return string(getpwuid(getuid())->pw_dir); }

string Tools::getTaraxaDefaultDir() { return getHomeDir() + "/" + DEFAULT_TARAXA_DIR_NAME; }

string Tools::getTaraxaDataDefaultDir() { return getHomeDir() + "/" + DEFAULT_TARAXA_DB_DIR_NAME; }

string Tools::getWalletDefaultFile() { return getTaraxaDefaultDir() + "/" + DEFAULT_WALLET_FILE_NAME; }

string Tools::getTaraxaDefaultConfigFile() { return getTaraxaDefaultDir() + "/" + DEFAULT_CONFIG_FILE_NAME; }

void Tools::writeJsonToFile(const string& file_name, const Json::Value& json) {
  ofstream ofile(file_name, ios::trunc);

  if (ofile.is_open()) {
    ofile << json;
  } else {
    stringstream err;
    err << "Cannot open file " << file_name << endl;
    throw invalid_argument(err.str());
  }
}

Json::Value Tools::readJsonFromFile(const string& file_name) {
  ifstream ifile(file_name);
  if (ifile.is_open()) {
    Json::Value json;
    ifile >> json;
    ifile.close();
    return json;
  } else {
    throw invalid_argument(string("Cannot open file ") + file_name);
  }
}

Json::Value Tools::readJsonFromString(const string& str) {
  Json::CharReaderBuilder builder;
  Json::CharReader* reader = builder.newCharReader();
  Json::Value json;
  std::string errors;

  bool parsingSuccessful = reader->parse(str.c_str(), str.c_str() + str.size(), &json, &errors);
  delete reader;

  if (!parsingSuccessful) {
    throw invalid_argument(string("Cannot parse json"));
  }

  return json;
}

}  // namespace taraxa::cli
