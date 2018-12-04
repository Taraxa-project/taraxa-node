/*
 * @Copyright: Taraxa.io 
 * @Author: Chia-Chun Lin 
 * @Date: 2018-11-02 14:19:58 
 * @Last Modified by: Chia-Chun Lin
 * @Last Modified time: 2018-12-04 13:54:29
 */
 
#ifndef FULL_NODE_HPP
#define FULL_NODE_HPP

#include <iostream>
#include <string>
#include <memory>
#include <vector>
#include <boost/asio.hpp>
#include "rocks_db.hpp"
#include "util.hpp"

namespace taraxa{

using std::string;
using std::to_string;

struct FullNodeConfig {
	FullNodeConfig (std::string const &json_file):json_file_name(json_file){
		rapidjson::Document doc = loadJsonFile(json_file);
		
		assert(doc.HasMember("port"));
		assert(doc.HasMember("address"));
		assert(doc.HasMember("db_accounts_path"));
		assert(doc.HasMember("db_blocks_path"));

		port = doc["port"].GetUint();
		address = boost::asio::ip::address::from_string(doc["address"].GetString());
		db_accounts_path = doc["db_accounts_path"].GetString();
		db_blocks_path = doc["db_blocks_path"].GetString();
	}
	std::string json_file_name;
	uint16_t port;
	boost::asio::ip::address address;
	std::string db_accounts_path;
	std::string db_blocks_path;
};


class FullNode : public std::enable_shared_from_this<FullNode> {
public:
	FullNode(FullNodeConfig const & conf):
		conf_(conf),
		db_accounts_sp_(std::make_shared<RocksDb> (conf_.db_accounts_path)),
		db_blocks_sp_(std::make_shared<RocksDb>(conf_.db_blocks_path)){
	}
	void setVerbose(bool verbose){
		verbose_=verbose;
	}
	// void createAccount(unsigned idx, unsigned long init_balance);
	void addRemotes(string ip, uint16_t port){
		remotes_.push_back({ip, port});
	}

	std::shared_ptr<FullNode> getShared() {return shared_from_this();}
	// void sendBlock(unsigned from, unsigned to, unsigned long new_balance);
	boost::asio::io_context & getIoContext() {return io_context_;}

	// Account related
	std::string accountCreate(name_t const & address);
	std::string accountQuery(name_t const & address);

	// Block related
	std::string blockCreate(blk_hash_t const & prev_hash, name_t const & from_address, 
		name_t const & to_address, uint64_t balance);
private:
	FullNodeConfig conf_;
	std::shared_ptr<RocksDb> db_accounts_sp_;
	std::shared_ptr<RocksDb> db_blocks_sp_;
	std::vector<std::pair<std::string, uint16_t>> remotes_; // neighbors for broadcasting
	bool verbose_=false;
	boost::asio::io_context io_context_;
};

}
#endif
