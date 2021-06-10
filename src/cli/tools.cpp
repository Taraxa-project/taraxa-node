#include "tools.hpp"

#include <pwd.h>

#include <filesystem>

#include "cli/config.hpp"
#include "devnet_config.hpp"
#include "testnet_config.hpp"

using namespace std;
using namespace dev;
namespace fs = std::filesystem;

namespace taraxa::cli {

void Tools::generateConfig(const std::string& config, const std::string& data_dir, bool boot_node, Config::NetworkIdType network_id) {
  Json::Value conf;
  switch (network_id) {
    case Config::NetworkIdType::Testnet:
      conf = readJsonFromString(testnet_json);
      break;
    case Config::NetworkIdType::Devnet:
      conf = readJsonFromString(devnet_json);
      break;
    default:
      throw invalid_argument("Unsupported network ID");
  }
  conf["db_path"] = data_dir;
  conf["network_is_boot_node"] = boot_node;

  writeJsonToFile(config, conf);
}

void Tools::generateWallet(const std::string& wallet) {
  // Wallet
  auto account = dev::KeyPair::create();
  auto [pk, sk] = taraxa::vrf_wrapper::getVrfKeyPair();
  auto account_json = createWalletJson(account, sk, pk);

  // Create account file
  writeJsonToFile(wallet, account_json);
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

}  // namespace taraxa
