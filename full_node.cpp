/*
 * @Copyright: Taraxa.io 
 * @Author: Chia-Chun Lin 
 * @Date: 2018-11-01 15:43:56 
 * @Last Modified by: Chia-Chun Lin
 * @Last Modified time: 2018-12-04 13:54:45
 */
 
#include <iostream>
#include <string>
#include <memory>
#include <boost/asio.hpp>
#include "full_node.hpp"
#include "user_account.hpp"
#include "state_block.hpp"
#include "rocks_db.hpp"

namespace taraxa{

	using std::string;
	using std::to_string;

	string FullNode::accountCreate(name_t const & address){
		if (!db_accounts_sp_->get(address).empty()) {
			return "Account already exists! \n";
		}
		UserAccount newAccount(address, "0", "0", 0, "0", 0);
		string jsonStr=newAccount.getJsonStr();
		db_accounts_sp_->put(address, jsonStr);
		return jsonStr;

	}
	string FullNode::accountQuery(name_t const & address){
		string jsonStr = db_accounts_sp_->get(address);
		if (jsonStr.empty()) {
			jsonStr = "Account does not exists! \n";
		}
		return jsonStr;
	}

	std::string FullNode::blockCreate(blk_hash_t const & prev_hash, name_t const & from_address, name_t const & to_address, bal_t balance){
		StateBlock newBlock(prev_hash, from_address, to_address, balance, "0", "0", "0");
		string jsonStr = newBlock.getJsonStr();
		return jsonStr;
	}


} // namespace taraxa
