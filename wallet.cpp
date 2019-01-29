/*
 * @Copyright: Taraxa.io 
 * @Author: Chia-Chun Lin 
 * @Date: 2018-12-03 23:37:53 
 * @Last Modified by: Chia-Chun Lin
 * @Last Modified time: 2019-01-28 22:46:25
 */
 
#include "wallet.hpp"
#include "signatures.hpp"
#include "rocks_db.hpp"

namespace taraxa{


WalletConfig::WalletConfig (std::string const & json_file):db_wallet_path (json_file){
	try{
		boost::property_tree::ptree doc = loadJsonFile(json_file);
		db_wallet_path = doc.get<std::string>("db_wallet_path");
		wallet_key = doc.get<std::string>("wallet_key"); 
	}
	catch (std::exception & e){
		std::cerr<<e.what()<<std::endl;
	}
}

WalletUserAccount::WalletUserAccount(std::string const & sk, std::string const & pk, std::string const & address): 
	sk(sk), pk(pk), address(address){}

WalletUserAccount::WalletUserAccount(std::string const & json){
	boost::property_tree::ptree doc = taraxa::strToJson(json);
	
	sk = doc.get<std::string>("sk");
	pk = doc.get<std::string>("pk");
	address = doc.get<std::string>("address");
}

Wallet::Wallet(WalletConfig const & conf): 
		conf_(conf),
		db_wallet_sp_(std::make_shared<RocksDb> (conf_.db_wallet_path)) {} 

std::string WalletUserAccount::getJsonStr(){
	boost::property_tree::ptree doc;
	
	doc.put("sk", sk);
	doc.put("pk", pk);
	doc.put("address", address);

	std::stringstream ostrm;
	boost::property_tree::write_json(ostrm, doc);
	return ostrm.str();
}

std::string Wallet::accountCreate (key_t sk){
	// bit length extension to match send/receive block API	
	for (unsigned int i=sk.size(); i<64; ++i) sk="0"+sk;
	// gen secret key, not use for now
	// sk_=taraxa::generate_private_key_from_seed(sd, 0, 0)[0];
	// gen public key
	auto key= taraxa::get_public_key_hex(sk);
	key_t pk=key[1];
	WalletUserAccount acc(sk, pk, pk);
	if (!db_wallet_sp_->get(acc.address).empty()) {return "Account already exist!\n";}
	std::string json=acc.getJsonStr();
	db_wallet_sp_->put(acc.address, json);
	return json;
}

std::string Wallet::accountQuery(name_t address){
	std::string acc = db_wallet_sp_->get(address);
	if (!acc.empty()){
		acc = "Account does not exist!\n";
	}
	return acc;
}

}  //namespace taraxa
