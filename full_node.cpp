/*
 * @Copyright: Taraxa.io 
 * @Author: Chia-Chun Lin 
 * @Date: 2018-11-01 15:43:56 
 * @Last Modified by: Chia-Chun Lin
 * @Last Modified time: 2019-01-10 11:56:14
 */

#include "full_node.hpp"
#include "user_account.hpp"
#include "state_block.hpp"
#include "rocks_db.hpp"
#include "network.hpp"
#include "block_processor.hpp"
#include <boost/asio.hpp>

namespace taraxa{

using std::string;
using std::to_string;

FullNodeConfig::FullNodeConfig (std::string const &json_file):json_file_name(json_file){
	rapidjson::Document doc = loadJsonFile(json_file);
	
	assert(doc.HasMember("udp_port"));
	assert(doc.HasMember("address"));
	assert(doc.HasMember("db_accounts_path"));
	assert(doc.HasMember("db_blocks_path"));
	assert(doc.HasMember("num_io_threads"));
	assert(doc.HasMember("num_packet_processing_threads"));

	udp_port = doc["udp_port"].GetUint();
	address = boost::asio::ip::address::from_string(doc["address"].GetString());
	db_accounts_path = doc["db_accounts_path"].GetString();
	db_blocks_path = doc["db_blocks_path"].GetString();
	num_io_threads = doc["num_io_threads"].GetUint();
	num_packet_processing_threads = doc["num_packet_processing_threads"].GetUint();
}

void FullNode::setVerbose(bool verbose){
	verbose_=verbose;
}

FullNode::FullNode(FullNodeConfig const & conf) try:
	conf_(conf),
	db_accounts_(std::make_shared<RocksDb> (conf_.db_accounts_path)),
	db_blocks_(std::make_shared<RocksDb>(conf_.db_blocks_path)), 
	network_(std::make_shared<Network>(io_context_, conf.udp_port)),
	blk_processor_(std::make_shared<BlockProcessor>(*this)){
} catch(std::exception &e){
	throw e;
} 

std::shared_ptr<FullNode> FullNode::getShared() {return shared_from_this();}
boost::asio::io_context & FullNode::getIoContext() {return io_context_;}

string FullNode::accountCreate(name_t const & address){
	if (!db_accounts_->get(address).empty()) {
		return "Account already exists! \n";
	}
	UserAccount newAccount(address, "0", "0", 0, "0", 0);
	string jsonStr=newAccount.getJsonStr();
	db_accounts_->put(address, jsonStr);
	return jsonStr;

}
string FullNode::accountQuery(name_t const & address){
	string jsonStr = db_accounts_->get(address);
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

const FullNodeConfig & FullNode::getConfig() const { return conf_;}

} // namespace taraxa
