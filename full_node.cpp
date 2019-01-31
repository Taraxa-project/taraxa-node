/*
 * @Copyright: Taraxa.io 
 * @Author: Chia-Chun Lin 
 * @Date: 2018-11-01 15:43:56 
 * @Last Modified by: Chia-Chun Lin
 * @Last Modified time: 2019-01-31 00:02:12
 */

#include <boost/asio.hpp>
#include "full_node.hpp"
#include "state_block.hpp"
#include "rocks_db.hpp"
#include "network.hpp"
#include "dag.hpp"
#include "block_proposer.hpp"

namespace taraxa {

using std::string;
using std::to_string;

FullNodeConfig::FullNodeConfig (std::string const &json_file):json_file_name(json_file){
	
	try{
		boost::property_tree::ptree doc = loadJsonFile(json_file);
		address = boost::asio::ip::address::from_string(doc.get<std::string>("address"));
		db_accounts_path = doc.get<std::string>("db_accounts_path");
		db_blocks_path = doc.get<std::string>("db_blocks_path");
		dag_processing_threads = doc.get<uint16_t>("dag_processing_threads");
		block_proposer_threads = doc.get<uint16_t>("block_proposer_threads");
	}
	catch(std::exception &e){
		std::cerr<<e.what()<<std::endl;
	}
}

void FullNode::setVerbose(bool verbose){
	verbose_=verbose;
	network_->setVerbose(verbose);
	dag_mgr_->setVerbose(verbose);
	db_blocks_->setVerbose(verbose);

}

void FullNode::setDebug(bool debug){
	debug_=debug;
	network_->setDebug(debug);
}

FullNode::FullNode(boost::asio::io_context & io_context, 
		std::string const & conf_full_node, 
		std::string const & conf_network) try:
	io_context_(io_context),
	conf_full_node_(conf_full_node), 
	conf_network_(conf_network),
	conf_(conf_full_node),
	db_accounts_(std::make_shared<RocksDb> (conf_.db_accounts_path)),
	db_blocks_(std::make_shared<RocksDb>(conf_.db_blocks_path)), 
	network_(std::make_shared<Network>(io_context_, conf_network)),
	dag_mgr_(std::make_shared<DagManager>(conf_.dag_processing_threads)),
	blk_proposer_(std::make_shared<BlockProposer>(conf_.block_proposer_threads, dag_mgr_->getShared())){
} catch(std::exception &e){
	std::cerr<<e.what()<<std::endl;
	throw e;
} 

std::shared_ptr<FullNode> FullNode::getShared() {
	try{
		return shared_from_this();
	} catch( std::bad_weak_ptr & e){
		std::cerr<<"FullNode: "<<e.what()<<std::endl;
		return nullptr;
	}
}
boost::asio::io_context & FullNode::getIoContext() {return io_context_;}

void FullNode::start(){
	network_->setFullNodeAndMsgParser(getShared());
	network_->start();
	dag_mgr_->start();
	blk_proposer_->start();
}

void FullNode::stop(){
	blk_proposer_->stop();
	network_->stop();
}

void FullNode::storeBlock(StateBlock const & blk){
	std::string key(blk.getHash().toString());
	if (verbose_){
		std::cout<<"Writing to block db ... "<<key<<std::endl;
	}
	bool inserted = db_blocks_->put(key, blk.getJsonStr());
	if (debug_){
		std::unique_lock<std::mutex> lock(debug_mutex_);
		received_blocks_++;
	}
	// uninserted means already exist, do not call dag 
	if (inserted){
		dag_mgr_->addStateBlock(blk, true);
	}
}

StateBlock FullNode::getDagBlock(blk_hash_t const & hash){
	std::string json = db_blocks_->get(hash.toString());
	if (json.empty()){
		return StateBlock();
	}
	else{
		return StateBlock(json);
	}
}

time_stamp_t FullNode::getDagBlockTimeStamp (blk_hash_t const & hash){
	return dag_mgr_->getStateBlockTimeStamp(hash.toString());
}


std::vector<std::string> FullNode::getDagBlockChildren(blk_hash_t const &hash, time_stamp_t stamp){
	std::vector<std::string> children = dag_mgr_->getChildrenBeforeTimeStamp(hash.toString(), stamp);
	return children;
}

std::vector<std::string> FullNode::getDagBlockTips(blk_hash_t const &hash, time_stamp_t stamp){
	std::vector<std::string> tips = dag_mgr_->getTipsBeforeTimeStamp(hash.toString(), stamp);
	return tips;
}

std::vector<std::string> FullNode::getDagBlockSiblings(blk_hash_t const &hash, time_stamp_t stamp){
	StateBlock blk = getDagBlock(hash);
	std::vector<std::string> parents;
	parents.emplace_back(blk.getPivot().toString());
	for (auto const & tip: blk.getTips()){
		parents.emplace_back(tip.toString());
	}
		
	std::vector<std::string> siblings;
	for (auto const & parent: parents){
		std::vector<std::string> children;
		children = dag_mgr_->getChildrenBeforeTimeStamp(parent, stamp);
		siblings.insert(siblings.end(), children.begin(), children.end());
	}
	return siblings;
}

uint64_t FullNode::getNumReceivedBlocks(){
	return received_blocks_;
}

uint64_t FullNode::getNumProposedBlocks(){
	return BlockProposer::getNumProposedBlocks();
}

uint64_t FullNode::getNumVerticesInDag(){
	return dag_mgr_->getNumVerticesInDag();
}

uint64_t FullNode::getNumEdgesInDag(){
	return dag_mgr_->getNumEdgesInDag();
}

void FullNode::drawGraph(std::string const & dotfile) const{
	dag_mgr_->drawGraph(dotfile);
}

FullNodeConfig const & FullNode::getConfig() const { return conf_;}
std::shared_ptr<Network> FullNode::getNetwork() const { return network_;}

} // namespace taraxa
