/*
 * @Copyright: Taraxa.io 
 * @Author: Chia-Chun Lin 
 * @Date: 2018-10-31 16:26:04 
 * @Last Modified by: Chia-Chun Lin
 * @Last Modified time: 2018-12-04 12:27:20
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
		
	prev_hash_= doc["prev_hash"].GetString();
	from_address_=doc["from_address"].GetString();
	to_address_=doc["to_address"].GetString();
	balance_=doc["balance"].GetUint64();
	work_=doc["work"].GetString();
	signature_=doc["signature"].GetString();
	matching_=doc["matching"].GetString();

}

std::string StateBlock::getJsonStr(){
	rapidjson::Document doc;
	doc.SetObject();

	auto& allocator = doc.GetAllocator();
	doc.AddMember("prev_hash", rapidjson::StringRef(prev_hash_.c_str()), allocator);
	doc.AddMember("from_address", rapidjson::StringRef(from_address_.c_str()), allocator);
	doc.AddMember("to_address", rapidjson::StringRef(from_address_.c_str()), allocator);
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
