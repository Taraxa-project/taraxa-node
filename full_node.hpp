/*
 * @Copyright: Taraxa.io 
 * @Author: Chia-Chun Lin 
 * @Date: 2018-11-02 14:19:58 
 * @Last Modified by: Chia-Chun Lin
 * @Last Modified time: 2019-01-30 23:42:31
 */
 
#ifndef FULL_NODE_HPP
#define FULL_NODE_HPP

#include <iostream>
#include <string>
#include <memory>
#include <vector>
#include <boost/asio.hpp>
#include "util.hpp"

namespace taraxa{

class RocksDb;
class Network;
class BlockProposer;
class DagManager;
class StateBlock;

struct FullNodeConfig {
	FullNodeConfig (std::string const &json_file);
	std::string json_file_name;
	boost::asio::ip::address address;
	std::string db_accounts_path;
	std::string db_blocks_path;
	uint16_t dag_processing_threads;
	uint16_t block_proposer_threads;
};


class FullNode : public std::enable_shared_from_this<FullNode> {
public:
	FullNode( boost::asio::io_context & io_context, 
		std::string const & conf_full_node, 
		std::string const & conf_network);
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
	
	// Dag query: return childern, siblings, tips before time stamp
	StateBlock getDagBlock(blk_hash_t const & hash);
	time_stamp_t getDagBlockTimeStamp (blk_hash_t const & hash);
	std::vector<std::string> getDagBlockChildren(blk_hash_t const &blk, time_stamp_t stamp);
	std::vector<std::string> getDagBlockSiblings(blk_hash_t const &blk, time_stamp_t stamp);
	std::vector<std::string> getDagBlockTips(blk_hash_t const &blk, time_stamp_t stamp);

	// debugger
	uint64_t getNumReceivedBlocks();
	uint64_t getNumProposedBlocks();
	uint64_t getNumVerticesInDag();
	uint64_t getNumEdgesInDag();
	void drawGraph(std::string const & dotfile) const;
private:
	// ** NOTE: io_context must be constructed before Network
	boost::asio::io_context & io_context_;
	// configure files
	std::string conf_full_node_;
	std::string conf_network_;
	
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

	// dag
	std::shared_ptr<DagManager> dag_mgr_;
	// block proposer (multi processing)
	std::shared_ptr<BlockProposer> blk_proposer_;

	std::vector<std::pair<std::string, uint16_t>> remotes_; // neighbors for broadcasting
	
	// debugger
	std::mutex debug_mutex_;
	uint64_t received_blocks_ = 0;

};

}
#endif
