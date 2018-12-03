/*
 * @Copyright: Taraxa.io 
 * @Author: Chia-Chun Lin 
 * @Date: 2018-10-31 16:26:37 
 * @Last Modified by: Chia-Chun Lin
 * @Last Modified time: 2018-11-27 17:12:18
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

namespace taraxa{
using std::string;

// Block definition
/*					Size		Description 
	prev_hash						hash of previous block
	address							the account address that create the block
	balance							resulting balance
	work							proof of work
	signature						signature 
	matching						receiver address for send block; sender for receive block
*/

class StateBlock{
public:

	StateBlock(string prev_hash, 
				string address, 
				uint64_t balance, 
				string work, 
				string signature, 
				string matching
				): prev_hash_(prev_hash), 
					address_(address), 
					balance_(balance), 
					work_(work),
					signature_(signature), 
					matching_(matching){}
	StateBlock(const string &json);
	
	friend std::ostream & operator<<(std::ostream &str, StateBlock &u){
		str<<"	prev_hash	= "<< u.prev_hash_ << std::endl;
		str<<"	address		= "<< u.address_ << std::endl;
		str<<"	balance		= "<< u.balance_ << std::endl;
		str<<"	work		= "<< u.work_<<std::endl;
		str<<"	signature	= "<< u.signature_<<std::endl;
		str<<"	matching	= "<< u.matching_<<std::endl;

		return str;
	}
	
	string getPrevHash() {return prev_hash_;}
	string getAddress() {return address_;}
	uint64_t getBalance() {return balance_;}
	string getWork() {return work_;}
	string getSignature() {return signature_;}
	string getMatching() {return matching_;}
	std::string getJson();
private:
	constexpr static unsigned nr_hash_chars = 2 * CryptoPP::BLAKE2s::DIGESTSIZE;
	
	string prev_hash_ = "0";
	string address_ = "0"; 
	uint64_t balance_ = 0;
	string work_ = "0";
	string signature_ = "0";
	string matching_ = "0";
};

} // namespace taraxa
#endif