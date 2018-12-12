/*
 * @Copyright: Taraxa.io 
 * @Author: Chia-Chun Lin 
 * @Date: 2018-11-27 14:53:42 
 * @Last Modified by: Chia-Chun Lin
 * @Last Modified time: 2018-12-04 11:36:49
 */
 
#ifndef USER_ACCOUNT
#define USER_ACCOUNT

#include <iostream> 
#include <string> 
#include "util.hpp"

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
	UserAccount(name_t address,	
				key_t pk, 
				blk_hash_t genesis, 
				bal_t balance,
				blk_hash_t frontier, 
				uint64_t height); 
		
	UserAccount(string const& json);
	string getJsonStr();

private:
	name_t address_ = "0";
	key_t pk_ = "0";
	blk_hash_t genesis_ = "0";
	bal_t balance_ = 0;
	blk_hash_t frontier_ = "0";
	uint64_t height_ = 0;
};

} //namespace taraxa

#endif
