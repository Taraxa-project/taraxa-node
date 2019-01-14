/*
 * @Copyright: Taraxa.io 
 * @Author: Chia-Chun Lin 
 * @Date: 2018-10-31 16:26:37 
 * @Last Modified by: Chia-Chun Lin
 * @Last Modified time: 2019-01-14 12:09:38
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

	StateBlock(blk_hash_t pivot, 
				vec_tip_t tips, 
				vec_trx_t trxs,
				sig_t signature, 
				blk_hash_t hash
				);
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

		return str;
	}
	
	blk_hash_t getPivot();
	vec_tip_t getTips(); 
	vec_trx_t getTrxs();
	sig_t getSignature();
	blk_hash_t getHash();
	std::string getJsonStr();

private:

	constexpr static unsigned nr_hash_chars = 2 * CryptoPP::BLAKE2s::DIGESTSIZE;
	
	blk_hash_t pivot_ = "0";
	vec_tip_t tips_ ; 
	vec_trx_t trxs_ ; // transactions
	sig_t signature_ = "0";
	blk_hash_t hash_ = "0";
};

} // namespace taraxa
#endif