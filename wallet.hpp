/*
 * @Copyright: Taraxa.io 
 * @Author: Chia-Chun Lin 
 * @Date: 2018-12-03 17:03:47 
 * @Last Modified by: Chia-Chun Lin
 * @Last Modified time: 2018-12-04 12:56:01
 */

#ifndef WALLET_HPP
#define WALLET_HPP

#include <iostream>
#include <string>
#include <memory>
#include <rapidjson/document.h>
#include "util.hpp"
#include "rocks_db.hpp"
#include "signatures.hpp"

namespace taraxa{

struct WalletConfig{
	WalletConfig (std::string const & json_file):db_wallet_path (json_file){
		rapidjson::Document doc = loadJsonFile(json_file);
		assert(doc.HasMember("db_wallet_path"));
		assert(doc.HasMember("wallet_key"));
		db_wallet_path = doc["db_wallet_path"].GetString();
		wallet_key = doc["wallet_key"].GetString();
	}
	
	std::string db_wallet_path;
	key_t wallet_key;
};

// triplet: sk, pk, address
struct WalletUserAccount{
	WalletUserAccount(std::string const & sk, std::string const & pk, std::string const & address): 
		sk(sk), pk(pk), address(address){}
	WalletUserAccount(std::string const &json);
	key_t sk;
	key_t pk;
	name_t address;
	std::string getJsonStr();

};

class Wallet : public std::enable_shared_from_this<Wallet> {
public:
	Wallet(WalletConfig const & conf): 
		conf_(conf),
		db_wallet_sp_(std::make_shared<RocksDb> (conf_.db_wallet_path)) {} 
	std::string accountCreate(key_t sk);
	std::string accountQuery(name_t address);
	std::shared_ptr<Wallet> getShared() {return shared_from_this();}
private: 
	WalletConfig conf_;
	key_t wallet_key_ = "0"; 
	std::shared_ptr<RocksDb> db_wallet_sp_;
};

} // namespace taraxa

#endif
