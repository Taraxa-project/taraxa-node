/*
 * @Copyright: Taraxa.io 
 * @Author: Chia-Chun Lin 
 * @Date: 2018-12-12 13:31:22 
 * @Last Modified by: Chia-Chun Lin 
 * @Last Modified time: 2018-12-12 13:31:22 
 */
 
#include "user_account.hpp"
#include <iostream>
#include <string>
#include <rapidjson/document.h>
#include <rapidjson/prettywriter.h>

using std::to_string;
using std::string;

namespace taraxa {

UserAccount::UserAccount(name_t address,	
	key_t pk, 
	blk_hash_t genesis, 
	bal_t balance,
	blk_hash_t frontier, 
	uint64_t height): 
	address_(address), 
	pk_(pk),
	genesis_(genesis),
	balance_(balance),
	frontier_(frontier),
	height_(height){}

UserAccount::UserAccount(const string &json){
	rapidjson::Document doc = taraxa::strToJson(json);

	address_ = doc["address"].GetString();
	pk_ = doc["pk"].GetString();
	genesis_ = doc["genesis"].GetUint64();
	balance_ = doc["balance"].GetUint64();
	frontier_ = doc["frontier"].GetString();
	height_ = doc["height"].GetUint64();
}

std::string UserAccount::getJsonStr(){
	rapidjson::Document doc;
	doc.SetObject();

	auto& allocator = doc.GetAllocator();
	doc.AddMember("address", rapidjson::StringRef(address_.c_str()), allocator);
	doc.AddMember("pk", rapidjson::StringRef(pk_.c_str()), allocator);
	doc.AddMember("genesis", rapidjson::StringRef(genesis_.c_str()), allocator);
	doc.AddMember("balance", rapidjson::Value().SetUint64(balance_), allocator);
	doc.AddMember("frontier", rapidjson::StringRef(frontier_.c_str()), allocator);
	doc.AddMember("height", rapidjson::Value().SetUint64(height_), allocator);

	rapidjson::StringBuffer buffer;
	rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
	doc.Accept(writer);
	return buffer.GetString();
}

} //namespace taraxa
