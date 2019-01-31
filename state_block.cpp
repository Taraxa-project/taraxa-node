/*
 * @Copyright: Taraxa.io 
 * @Author: Chia-Chun Lin 
 * @Date: 2018-10-31 16:26:04 
 * @Last Modified by: Chia-Chun Lin
 * @Last Modified time: 2019-01-31 00:08:40
 */
#include "state_block.hpp"
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
name_t StateBlock::getPublisher() const {return publisher_;}

StateBlock::StateBlock(blk_hash_t pivot, 
	vec_tip_t tips, 
	vec_trx_t trxs,
	sig_t signature, 
	blk_hash_t hash,
	name_t publisher
	): 
	pivot_(pivot), 
	tips_(tips), 
	trxs_(trxs), 
	signature_(signature), 
	hash_(hash),
	publisher_(publisher){

}
StateBlock::StateBlock(stream &strm){
	deserialize(strm);
}
StateBlock::StateBlock(std::string const &json){
	try{
		boost::property_tree::ptree doc = strToJson(json);
		pivot_= doc.get<std::string> ("pivot");
		tips_ = asVector<blk_hash_t, std::string>(doc, "tips");
		trxs_ = asVector<trx_hash_t, std::string>(doc, "trxs");
		signature_ = doc.get<std::string> ("sig");
		hash_ = doc.get<std::string>("hash");
		publisher_ = doc.get<std::string>("pub");
	} 
	catch (std::exception &e) {
		std::cerr<<e.what()<<std::endl;
	}
}
bool StateBlock::isValid() const {
	return !(pivot_.isZero() && hash_.isZero() &&
		signature_.isZero() && publisher_.isZero());

}

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

	tree.put("sig", signature_.toString());
	tree.put("hash", hash_.toString());
	tree.put("pub", publisher_.toString());

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
	ok &= write(strm, publisher_);
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
	ok &= read(strm, publisher_);
	assert(ok);
	return ok;
}

bool StateBlock::operator== (StateBlock const & other) const{
	return this->getJsonStr() == other.getJsonStr();
}

}  //namespace
