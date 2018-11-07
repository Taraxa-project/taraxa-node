/*
 * @Copyright: Taraxa.io 
 * @Author: Chia-Chun Lin 
 * @Date: 2018-10-31 16:26:37 
 * @Last Modified by: Chia-Chun Lin
 * @Last Modified time: 2018-11-02 18:00:22
 */

#ifndef WALLET_HPP
#define WALLET_HPP

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

class UserAccount{
public:
	UserAccount(unsigned seed, unsigned long balance):
		seed_(seed),
		balance_(balance){
		// bit length extension to match send/receive block API
		std::string sd=std::to_string(seed);
		for (unsigned int i=sd.size(); i<64; ++i) sd="0"+sd;
		sk_=sd;
		for (unsigned int i=prev_hash_.size(); i<nr_hash_chars; ++i) prev_hash_+='0';
		// gen secret key, not use for now
		// sk_=taraxa::generate_private_key_from_seed(sd, 0, 0)[0];
		// gen public key
		auto key= taraxa::get_public_key_hex(sd);
		pk_=key[1];
	}
	UserAccount(const std::string &json);
	std::string sendBlock(std::string receiver, unsigned new_balance);
	friend std::ostream & operator<<(std::ostream &str, UserAccount &u){
		str<<"User Account seed       = "<< u.seed_ << std::endl;
		str<<"             secret key = "<< u.sk_ << std::endl;
		str<<"             public key = "<< u.pk_ << std::endl;
		str<<"             balance    = "<< u.balance_ << std::endl;
		str<<"             prev hash  = "<< u.prev_hash_ << std::endl;
		return str;
	}
	unsigned getSeed(){return seed_;}
	unsigned long getBalance() {return balance_;}
	std::string getPubKey() {return pk_;}
	std::string getJson();
private:
	unsigned nr_hash_chars = 2 * CryptoPP::BLAKE2s::DIGESTSIZE;
	unsigned seed_;
	std::string sk_;
	std::string pk_;
	unsigned long balance_ = 0;
	std::string prev_hash_ = "0";
};

#endif