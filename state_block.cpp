/*
 * @Copyright: Taraxa.io 
 * @Author: Chia-Chun Lin 
 * @Date: 2018-10-31 16:26:04 
 * @Last Modified by: Chia-Chun Lin
 * @Last Modified time: 2018-11-27 15:21:27
 */
 
#include <iostream>
#include <string>
#include <rapidjson/document.h>
#include <rapidjson/prettywriter.h>
#include "state_block.hpp"
#include "util.hpp"

namespace taraxa{

using std::to_string;
StateBlock::StateBlock(std::string const &json){

	rapidjson::Document doc = taraxa::strToJson(json);
	doc.Parse(json.c_str());
	
	prev_hash_= doc["prev_hash"].GetString();
	address_=doc["address"].GetString();
	balance_=doc["balance"].GetUint64();
	work_=doc["work"].GetString();
	signature_=doc["signature"].GetString();
	matching_=doc["matching"].GetString();

}

std::string StateBlock::getJson(){
	rapidjson::Document doc;
	doc.SetObject();

	auto& allocator = doc.GetAllocator();
	doc.AddMember("prev_hash", rapidjson::StringRef(prev_hash_.c_str()), allocator);
	doc.AddMember("address", rapidjson::StringRef(address_.c_str()), allocator);
	doc.AddMember("balance", rapidjson::Value().SetUint64(balance_), allocator);
	doc.AddMember("work", rapidjson::StringRef(work_.c_str()), allocator);
	doc.AddMember("signature", rapidjson::StringRef(signature_.c_str()), allocator);
	doc.AddMember("matching", rapidjson::StringRef(matching_.c_str()), allocator);

	rapidjson::StringBuffer buffer;
	rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
	doc.Accept(writer);
	return buffer.GetString();
}

}
