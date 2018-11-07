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
    seed_=std::stoul(document["seed"].GetString());
	pk_=document["public-key"].GetString();
	sk_=document["private-key"].GetString();
	balance_=std::stoul(document["balance"].GetString());
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
	document.AddMember("seed", rapidjson::StringRef(std::to_string(seed_).c_str()), allocator);
	document.AddMember("public-key", rapidjson::StringRef(pk_.c_str()), allocator);
	document.AddMember("private-key", rapidjson::StringRef(sk_.c_str()), allocator);
	document.AddMember("balance", rapidjson::StringRef(std::to_string(balance_).c_str()), allocator);
	document.AddMember("prev-hash", rapidjson::StringRef(prev_hash_.c_str()), allocator);

	rapidjson::StringBuffer buffer;
	rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
	document.Accept(writer);
	return buffer.GetString();
}
// int main(int argc, char *argv[]){
// 	std::vector<UserAccount> accounts = {UserAccount(1, 1024), UserAccount(2, 2048), UserAccount(3, 4096)};
// 	TcpClient tcp_client;
// 	tcp_client.setVerbose(true);
// 	tcp_client.connect("192.168.12.204", 3000);
// 	for (int i=0; i<accounts.size()-1; ++i){
// 		UserAccount &a=accounts[i];
// 		unsigned sd=a.getSeed();
// 		tcp_client.echo(a.sendBlock(taraxa::all_users[sd+1], a.getBalance()>>1));
// 		//break;
// 	}
// 	tcp_client.close();
// 	for (auto a:accounts) std::cout<<a<<std::endl;
// 	return 1;
// }