/*
 * @Copyright: Taraxa.io 
 * @Author: Chia-Chun Lin 
 * @Date: 2018-10-31 16:26:04 
 * @Last Modified by: Chia-Chun Lin
 * @Last Modified time: 2019-01-14 12:40:52
 */
#include "state_block.hpp"
#include <rapidjson/document.h>
#include <rapidjson/prettywriter.h>

namespace taraxa{

using std::to_string;

blk_hash_t StateBlock::getPivot() {return pivot_;}
vec_tip_t StateBlock::getTips() {return tips_;}
vec_trx_t StateBlock::getTrxs() {return trxs_;}
sig_t StateBlock::getSignature() {return signature_;}
blk_hash_t StateBlock::getHash() {return hash_;}

StateBlock::StateBlock(blk_hash_t pivot, 
	vec_tip_t tips, 
	vec_trx_t trxs,
	sig_t signature, 
	blk_hash_t hash
	): 
	pivot_(pivot), 
	tips_(tips), 
	trxs_(trxs), 
	signature_(signature), 
	hash_(hash){

}

StateBlock::StateBlock(std::string const &json){

	using namespace rapidjson;
	Document doc = taraxa::strToJson(json);
	
	assert(doc["pivot"].IsString());
	assert(doc["tips"].IsArray());
	assert(doc["trxs"].IsArray());
	assert(doc["signature"].IsString());
	assert(doc["hash"].IsString());
	
	pivot_= doc["pivot"].GetString();
	const Value & tips = doc["tips"];
	for (SizeType i = 0; i < tips.Size(); ++i){
		assert(tips[i].IsString());
		tips_.push_back(tips[i].GetString());
	}
	const Value & trxs = doc["trxs"];
	for (SizeType i = 0; i < trxs.Size(); ++i){
		assert(trxs[i].IsString());
		trxs_.push_back(trxs[i].GetString());
	}
	signature_=doc["signature"].GetString();
	hash_=doc["hash"].GetString();

}

std::string StateBlock::getJsonStr() const{
	
	using namespace rapidjson;
	Document doc;
	doc.SetObject();

	Value tips(kArrayType);
	Value trxs(kArrayType);
	auto& allocator = doc.GetAllocator();
	for (auto i = 0; i < tips_.size(); ++i){
		tips.PushBack(StringRef(tips_[i].c_str()),allocator);
	}
	for (auto i = 0; i < trxs_.size(); ++i){
		trxs.PushBack(StringRef(trxs_[i].c_str()),allocator);
	}	

	doc.AddMember("pivot", StringRef(pivot_.c_str()), allocator);
	doc.AddMember("tips", tips, allocator);
	doc.AddMember("trxs", trxs, allocator);	
	doc.AddMember("signature", StringRef(signature_.c_str()), allocator);
	doc.AddMember("hash", StringRef(hash_.c_str()), allocator);

	rapidjson::StringBuffer buffer;
	rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
	doc.Accept(writer);
	return buffer.GetString();
}

}  //namespace
