/*
 * @Copyright: Taraxa.io 
 * @Author: Chia-Chun Lin 
 * @Date: 2018-10-31 16:26:37 
 * @Last Modified by: Chia-Chun Lin
 * @Last Modified time: 2018-12-10 12:11:31
 */

#ifndef STATE_BLOCK_HPP
#define STATE_BLOCK_HPP

#include <iostream>
#include <string>
#include <cryptopp/aes.h>
#include <cryptopp/blake2.h>
#include <rapidjson/document.h>
#include <unordered_map>
#include <vector>
#include "rocks_db.hpp"
#include "signatures.hpp"
#include "transactions.hpp"
#include "util.hpp"

namespace taraxa{
using std::string;

// Block definition
/*					Size		Description 
	prev_hash						hash of previous block
	from_address					from account address that create the block
	to_address						to account address  
	balance							resulting balance
	work							proof of work
	signature						signature 
	matching						receiver address for send block; sender for receive block
*/

class StateBlock{
public:

	StateBlock(blk_hash_t prev_hash, 
				name_t from_address, 
				name_t to_address,
				uint64_t balance, 
				nonce_t work, 
				sig_t signature, 
				blk_hash_t matching
				): prev_hash_(prev_hash), 
					from_address_(from_address), 
					to_address_(to_address), 
					balance_(balance), 
					work_(work),
					signature_(signature), 
					matching_(matching){}
	StateBlock(const string &json);
	
	friend std::ostream & operator<<(std::ostream &str, StateBlock &u){
		str<<"	prev_hash	= "<< u.prev_hash_ << std::endl;
		str<<"	from		= "<< u.from_address_ << std::endl;
		str<<"	to			= "<< u.to_address_ <<std::endl;
		str<<"	balance		= "<< u.balance_ << std::endl;
		str<<"	work		= "<< u.work_<<std::endl;
		str<<"	signature	= "<< u.signature_<<std::endl;
		str<<"	matching	= "<< u.matching_<<std::endl;

		return str;
	}
	
	blk_hash_t getPrevHash() {return prev_hash_;}
	name_t getFrom() {return from_address_;}
	name_t getTo() {return to_address_;}
	bal_t getBalance() {return balance_;}
	nonce_t getWork() {return work_;}
	sig_t getSignature() {return signature_;}
	blk_hash_t getMatching() {return matching_;}
	blk_hash_t getHash() {return "Not implement yet\n";}
	std::string getJsonStr();

private:

	constexpr static unsigned nr_hash_chars = 2 * CryptoPP::BLAKE2s::DIGESTSIZE;
	
	blk_hash_t prev_hash_ = "0";
	name_t from_address_ = "0"; 
	name_t to_address_ = "0"; 
	bal_t balance_ = 0;
	nonce_t work_ = "0";
	sig_t signature_ = "0";
	blk_hash_t matching_ = "0";
};

} // namespace taraxa
#endif