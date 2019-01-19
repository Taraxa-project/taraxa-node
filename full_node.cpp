/*
 * @Copyright: Taraxa.io 
 * @Author: Chia-Chun Lin 
 * @Date: 2018-11-01 15:43:56 
 * @Last Modified by: Chia-Chun Lin
 * @Last Modified time: 2019-01-18 17:25:13
 */

#include <boost/asio.hpp>
#include "full_node.hpp"
#include "state_block.hpp"
#include "rocks_db.hpp"
#include "network.hpp"


namespace taraxa{

using std::string;
using std::to_string;

FullNodeConfig::FullNodeConfig (std::string const &json_file):json_file_name(json_file){
	rapidjson::Document doc = loadJsonFile(json_file);
	
	assert(doc.HasMember("address"));
	assert(doc.HasMember("db_accounts_path"));
	assert(doc.HasMember("db_blocks_path"));

	address = boost::asio::ip::address::from_string(doc["address"].GetString());
	db_accounts_path = doc["db_accounts_path"].GetString();
	db_blocks_path = doc["db_blocks_path"].GetString();

}

void FullNode::setVerbose(bool verbose){
	verbose_=verbose;
	network_->setVerbose(verbose);
}

void FullNode::setDebug(bool debug){
	debug_=debug;
	network_->setDebug(debug);
}

FullNode::FullNode(boost::asio::io_context & io_context, 
		std::string const & conf_full_node, 
		std::string const & conf_network, 
		std::string const & conf_rpc) try:
	io_context_(io_context),
	conf_full_node_(conf_full_node), 
	conf_network_(conf_network),
	conf_rpc_(conf_rpc),
	conf_(conf_full_node),
	db_accounts_(std::make_shared<RocksDb> (conf_.db_accounts_path)),
	db_blocks_(std::make_shared<RocksDb>(conf_.db_blocks_path)), 
	network_(std::make_shared<Network>(io_context_, conf_network)){
} catch(std::exception &e){
	std::cerr<<e.what()<<std::endl;
	throw e;
} 

std::shared_ptr<FullNode> FullNode::getShared() {return shared_from_this();}
boost::asio::io_context & FullNode::getIoContext() {return io_context_;}

void FullNode::start(){
	network_->setFullNodeAndMsgParser(getShared());
	network_->start();
}

void FullNode::stop(){
	network_->stop();
}

void FullNode::storeBlock(StateBlock const & blk){
	std::string key(blk.getHash().toString());
	if (verbose_){
		std::cout<<"Writing to block db ... "<<key<<std::endl;
	}
	db_blocks_->put(key, blk.getJsonStr());
	if (debug_){
		std::unique_lock<std::mutex> lock(debug_mutex_);
		received_blocks_++;
	}
}

uint64_t FullNode::getNumReceivedBlocks(){
	return received_blocks_;
}

// string FullNode::accountCreate(name_t const & address){
// 	if (!db_accounts_->get(address).empty()) {
// 		return "Account already exists! \n";
// 	}
// 	UserAccount newAccount(address, "0", "0", 0, "0", 0);
// 	string jsonStr=newAccount.getJsonStr();
// 	db_accounts_->put(address, jsonStr);
// 	return jsonStr;

// }
// string FullNode::accountQuery(name_t const & address){
// 	string jsonStr = db_accounts_->get(address);
// 	if (jsonStr.empty()) {
// 		jsonStr = "Account does not exists! \n";
// 	}
// 	return jsonStr;
// }

// std::string FullNode::blockCreate(blk_hash_t const & prev_hash, name_t const & from_address, name_t const & to_address, bal_t balance){
// 	StateBlock newBlock(prev_hash, from_address, to_address, balance, "0", "0", "0");
// 	string jsonStr = newBlock.getJsonStr();
// 	return jsonStr;
// }

FullNodeConfig const & FullNode::getConfig() const { return conf_;}
std::shared_ptr<Network> FullNode::getNetwork() const { return network_;}
} // namespace taraxa
