/*
 * @Copyright: Taraxa.io 
 * @Author: Chia-Chun Lin 
 * @Date: 2018-11-27 14:53:42 
 * @Last Modified by: Chia-Chun Lin
 * @Last Modified time: 2018-11-27 15:35:12
 */
 
#ifndef USER_ACCOUNT
#define USER_ACCOUNT

#include <iostream> 
#include <string> 
#include "state_block.hpp"

namespace taraxa{
using std::string;

// User Account definition
/*					size		Description 
	address							Account address
	pk								Account public key
	genesis							First block of the account
	balance							Latest balance
	frontier						Last block of the account
	height							Height of last block
*/

class UserAccount{
public:
	UserAccount(string address,	
				string pk, 
				string genesis, 
				uint64_t balance,
				string frontier, 
				uint64_t height): 
					address_(address), 
					pk_(pk),
					genesis_(genesis),
					balance_(balance),
					frontier_(frontier),
					height_(height){}
	UserAccount(const string &json);
	string getJson();

private:
	string address_ = "0";
	string pk_ = "0";
	string genesis_ = "0";
	uint64_t balance_ = 0;
	string frontier_ = "0";
	uint64_t height_ = 0;
};

} //namespace taraxa

#endif
