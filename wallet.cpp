/*
 * @Copyright: Taraxa.io 
 * @Author: Chia-Chun Lin 
 * @Date: 2018-12-03 23:37:53 
 * @Last Modified by: Chia-Chun Lin
 * @Last Modified time: 2018-12-04 12:57:54
 */
 
#include "wallet.hpp"
//#include "transactions.hpp"

namespace taraxa{

WalletUserAccount::WalletUserAccount(std::string const & json){
	rapidjson::Document doc = taraxa::strToJson(json);
	
	sk = doc["sk"].GetString();
	pk = doc["pk"].GetString();
	address =doc["address"].GetString();
}

std::string WalletUserAccount::getJsonStr(){
	rapidjson::Document doc;
	doc.SetObject();

	auto& allocator = doc.GetAllocator();
	doc.AddMember("sk", rapidjson::StringRef(sk.c_str()), allocator);
	doc.AddMember("pk", rapidjson::StringRef(pk.c_str()), allocator);
	doc.AddMember("address", rapidjson::StringRef(address.c_str()), allocator);
	rapidjson::StringBuffer buffer;
	rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
	doc.Accept(writer);
	return buffer.GetString();
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
