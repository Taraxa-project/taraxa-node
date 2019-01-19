/*
 * @Copyright: Taraxa.io 
 * @Author: Chia-Chun Lin 
 * @Date: 2018-11-02 14:19:58 
 * @Last Modified by: Chia-Chun Lin
 * @Last Modified time: 2019-01-18 17:05:41
 */
 
#ifndef FULL_NODE_HPP
#define FULL_NODE_HPP

#include <iostream>
#include <string>
#include <memory>
#include <vector>
#include <boost/asio.hpp>
#include "util.hpp"
#include "rocks_db.hpp"
#include "state_block.hpp"

namespace taraxa{

class RocksDb;
class Network;
class BlockProcessor;

struct FullNodeConfig {
	FullNodeConfig (std::string const &json_file);
	std::string json_file_name;
	boost::asio::ip::address address;
	std::string db_accounts_path;
	std::string db_blocks_path;
};


class FullNode : public std::enable_shared_from_this<FullNode> {
public:
	FullNode( boost::asio::io_context & io_context, 
		std::string const & conf_full_node, 
		std::string const & conf_network, 
		std::string const & conf_rpc);
	void setVerbose(bool verbose);
	void setDebug(bool debug);
	void start();
	void stop();
	// ** Note can be called only FullNode is fully settled!!!
	std::shared_ptr<FullNode> getShared(); 
	boost::asio::io_context & getIoContext(); 

	FullNodeConfig const & getConfig() const;
	std::shared_ptr<Network> getNetwork() const;

	// Store a block in persistent storage and build in dag
	void storeBlock(StateBlock const &block);

	// Account related
	// std::string accountCreate(name_t const & address);
	// std::string accountQuery(name_t const & address);

	// Block related
	// std::string blockCreate(blk_hash_t const & prev_hash, name_t const & from_address, 
	// 	name_t const & to_address, uint64_t balance);

	// debugger
	uint64_t getNumReceivedBlocks();

private:
	// ** NOTE: io_context must be constructed before Network
	boost::asio::io_context & io_context_;
	// configure files
	std::string conf_full_node_;
	std::string conf_network_;
	std::string conf_rpc_;
	
	// configuration
	FullNodeConfig conf_;
	bool verbose_=false;
	bool debug_=false;
	
	//storage 
	std::shared_ptr<RocksDb> db_accounts_;
	std::shared_ptr<RocksDb> db_blocks_;

	// network
	std::shared_ptr<Network> network_;
	// ledger

	// block processor (multi processing)
	// std::shared_ptr<BlockProcessor> blk_processor_;

	std::vector<std::pair<std::string, uint16_t>> remotes_; // neighbors for broadcasting
	
	// debugger
	std::mutex debug_mutex_;
	uint64_t received_blocks_ = 0;

};

}
#endif
