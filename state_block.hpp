/*
 * @Copyright: Taraxa.io 
 * @Author: Chia-Chun Lin 
 * @Date: 2018-10-31 16:26:37 
 * @Last Modified by: Chia-Chun Lin
 * @Last Modified time: 2019-01-30 10:36:37
 */

#ifndef STATE_BLOCK_HPP
#define STATE_BLOCK_HPP

#include <iostream>
#include <string>
#include <cryptopp/aes.h>
#include <cryptopp/blake2.h>
#include "util.hpp"

namespace taraxa{
using std::string;

// Block definition

class StateBlock{
public:
	StateBlock() = default;
	StateBlock(blk_hash_t pivot, 
				vec_tip_t tips, 
				vec_trx_t trxs,
				sig_t signature, 
				blk_hash_t hash,
				name_t publisher
				);
	StateBlock(stream &strm);
	StateBlock(const string &json);
	
	friend std::ostream & operator<<(std::ostream &str, StateBlock &u){
		str<<"	pivot		= "<< u.pivot_ << std::endl;
		str<<"	tips		= ";
		for (auto const &t: u.tips_)
			str<< t <<" ";
		str<<std::endl;
		str<<"	trxs		= ";
		for (auto const &t: u.trxs_)
			str<< t <<" ";
		str<<std::endl;
		str<<"	signature	= "<< u.signature_<<std::endl;
		str<<"	hash		= "<< u.hash_<<std::endl;
		str<<"  publisher   = "<< u.publisher_<<std::endl;

		return str;
	}
	bool operator== (StateBlock const & other) const;
	blk_hash_t getPivot() const;
	vec_tip_t getTips() const ; 
	vec_trx_t getTrxs() const;
	sig_t getSignature() const;
	blk_hash_t getHash() const;
	name_t getPublisher() const;
	std::string getJsonStr() const;
	bool isValid() const;
	bool serialize (stream & strm) const;
	bool deserialize (stream & strm);

private:

	constexpr static unsigned nr_hash_chars = 2 * CryptoPP::BLAKE2s::DIGESTSIZE;
	
	blk_hash_t pivot_;
	vec_tip_t tips_; 
	vec_trx_t trxs_; // transactions
	sig_t signature_;
	blk_hash_t hash_;
	name_t publisher_; //block creater
};

} // namespace taraxa
#endif