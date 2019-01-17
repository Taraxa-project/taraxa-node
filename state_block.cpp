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
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <utility>

namespace taraxa{

using std::to_string;

blk_hash_t StateBlock::getPivot() const {return pivot_;}
vec_tip_t StateBlock::getTips() const {return tips_;}
vec_trx_t StateBlock::getTrxs() const {return trxs_;}
sig_t StateBlock::getSignature() const {return signature_;}
blk_hash_t StateBlock::getHash() const {return hash_;}

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
StateBlock::StateBlock(stream &strm){
	deserialize(strm);
}
StateBlock::StateBlock(std::string const &json){

	using namespace rapidjson;
	Document doc = taraxa::strToJson(json);
	
	assert(doc["pivot"].IsString());
	assert(doc["tips"].IsArray());
	assert(doc["trxs"].IsArray());
	assert(doc["signature"].IsString());
	assert(doc["hash"].IsString());
	
	pivot_= blk_hash_t(doc["pivot"].GetString());
	const Value & tips = doc["tips"];
	for (SizeType i = 0; i < tips.Size(); ++i){
		assert(tips[i].IsString());
		tips_.push_back(blk_hash_t(tips[i].GetString()));
	}
	const Value & trxs = doc["trxs"];
	for (SizeType i = 0; i < trxs.Size(); ++i){
		assert(trxs[i].IsString());
		trxs_.push_back(trx_hash_t(trxs[i].GetString()));
	}
	signature_=sig_t(doc["signature"].GetString());
	hash_=blk_hash_t(doc["hash"].GetString());

}

// rapidjson implementation, won't work, doesn't know why

// std::string StateBlock::getJsonStr() const{
	
// 	using namespace rapidjson;
// 	Document doc;
// 	doc.SetObject();

// 	Value tips(kArrayType);
// 	Value trxs(kArrayType);
// 	auto& allocator = doc.GetAllocator();
// 	for (auto i = 0; i < tips_.size(); ++i){
// 		std::string tmp(tips_[i].toString());
// 		tips.PushBack(StringRef(tmp.c_str()),allocator);
// 	}
// 	for (auto i = 0; i < trxs_.size(); ++i){
// 		std::string tmp(trxs_[i].toString());
// 		trxs.PushBack(StringRef(tmp.c_str()),allocator);
// 	}	

// 	doc.AddMember("pivot", StringRef(pivot_.toString().c_str()), allocator);
// 	doc.AddMember("tips", tips, allocator);
// 	doc.AddMember("trxs", trxs, allocator);	
// 	doc.AddMember("signature", StringRef(signature_.toString().c_str()), allocator);
// 	doc.AddMember("hash", StringRef(hash_.toString().c_str()), allocator);

// 	rapidjson::StringBuffer buffer;
// 	rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
// 	doc.Accept(writer);
// 	return buffer.GetString();
// }

std::string StateBlock::getJsonStr() const{
	using boost::property_tree::ptree;

	ptree tree;
	tree.put("pivot", pivot_.toString());
	
	tree.put_child("tips", ptree());
	auto &tips_array = tree.get_child("tips");
	for (auto const & t: tips_){
		tips_array.push_back(std::make_pair("", ptree(t.toString().c_str())));
	}

	tree.put_child("trxs", ptree());
	auto &trxs_array = tree.get_child("trxs");
	for (auto const & t: trxs_){
		trxs_array.push_back(std::make_pair("", ptree(t.toString().c_str())));
	}

	tree.put("signature", signature_.toString());
	tree.put("hash", hash_.toString());
	std::stringstream ostrm;
	boost::property_tree::write_json(ostrm, tree);
	return ostrm.str();
}

bool StateBlock::serialize(stream &strm) const{
	bool ok = true;
	uint8_t num_tips = tips_.size();
	uint8_t num_trxs = trxs_.size();
	ok &= write(strm, num_tips);
	ok &= write(strm, num_trxs);
	ok &= write(strm, pivot_);
	for (auto i = 0; i < num_tips; ++i){
		ok &= write(strm, tips_[i]);
	}
	for (auto i = 0; i < num_trxs; ++i){
		ok &= write(strm, trxs_[i]);
	}
	ok &= write(strm, signature_);
	ok &= write(strm, hash_);
	assert(ok);
	return ok;
}

bool StateBlock::deserialize(stream &strm){
	uint8_t num_tips, num_trxs;
	bool ok = true;

	ok &= read(strm, num_tips);
	ok &= read(strm, num_trxs);
	ok &= read(strm, pivot_);
	for (auto i = 0; i < num_tips; ++i){
		blk_hash_t t;
		ok &= read(strm, t);
		if (ok){
			tips_.push_back(t);
		}

	}
	for (auto i = 0; i < num_trxs; ++i){
		trx_hash_t t;
		ok &= read(strm, t);
		if (ok){
			trxs_.push_back(t);
		}
	}
	ok &= read(strm, signature_);
	ok &= read(strm, hash_);
	assert(ok);
	return ok;
}

}  //namespace
