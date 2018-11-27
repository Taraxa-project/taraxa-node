/*
 * @Copyright: Taraxa.io 
 * @Author: Chia-Chun Lin 
 * @Date: 2018-10-31 16:26:04 
 * @Last Modified by: Chia-Chun Lin
 * @Last Modified time: 2018-11-02 16:20:57
 */
 
#include <iostream>
#include <string>
#include <cryptopp/aes.h>
#include <cryptopp/blake2.h>
#include <rapidjson/document.h>
#include <rapidjson/prettywriter.h>
#include <unordered_map>
#include <vector>
#include "wallet.hpp"
#include "rocks_db.hpp"
#include "signatures.hpp"
#include "create_send.hpp"
#include "transactions.hpp"
#include "tcp_client.hpp"
#include "tcp_server.hpp"

using std::to_string;
UserAccount::UserAccount(const std::string &json){
	rapidjson::Document document;
	document.Parse(json.c_str());
	if (document.HasParseError()) {
		throw std::invalid_argument(
			__FILE__ "(" + to_string(__LINE__) + ") "
			"Couldn't parse json data at character position " + to_string(document.GetErrorOffset())
			+ ": " + rapidjson::GetParseError_En(document.GetParseError())
		);
	}

	if (not document.IsObject()) {
		throw std::invalid_argument(
			__FILE__ "(" + to_string(__LINE__) + ") "
			"JSON data doesn't represent an object ( {...} )"
		);
	}
	
	seed_= document["seed"].GetInt();
	pk_=document["public-key"].GetString();
	sk_=document["private-key"].GetString();
	balance_=document["balance"].GetUint64();
	prev_hash_=document["prev-hash"].GetString();

}
std::string UserAccount::sendBlock(std::string receiver, unsigned new_balance){

	if (new_balance >= balance_){
		std::cout<<"Cannot send, current balance = "<<balance_<<" , new balance = "<<new_balance<<std::endl;
		return "";
	}
	// update balance 
	balance_=new_balance;

	std::string blk=taraxa::create_send(sk_, receiver, std::to_string(new_balance), "0", prev_hash_);
	// TODO: rewrite, for now just open json ...
	rapidjson::Document document;
	document.Parse(blk.c_str());
	prev_hash_=document["hash"].GetString();
	return blk;
}
std::string UserAccount::getJson(){
	rapidjson::Document document;
	document.SetObject();

	auto& allocator = document.GetAllocator();
	document.AddMember("seed", rapidjson::Value().SetUint(seed_), allocator);
	document.AddMember("public-key", rapidjson::StringRef(pk_.c_str()), allocator);
	document.AddMember("private-key", rapidjson::StringRef(sk_.c_str()), allocator);
	document.AddMember("balance", rapidjson::Value().SetUint64(balance_), allocator);
	document.AddMember("prev-hash", rapidjson::StringRef(prev_hash_.c_str()), allocator);

	rapidjson::StringBuffer buffer;
	rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
	document.Accept(writer);
	return buffer.GetString();
}
