/*
 * @Copyright: Taraxa.io 
 * @Author: Chia-Chun Lin 
 * @Date: 2018-12-07 18:04:02 
 * @Last Modified by: Chia-Chun Lin
 * @Last Modified time: 2018-12-12 13:47:09
 */

#ifndef BLOCK_PROCESSOR_HPP
#define BLOCK_PROCESSOR_HPP
#include <iostream>
#include <unordered_set>
#include <mutex>
#include <queue>
#include "util.hpp"

// Process blocks with multithreading
namespace taraxa{

const unsigned int MAX_STATE_BLOCKS = 16384;
class FullNode;
class DagBlock;
// Call procedure
// processBlocks --> processManyBlocks --> batchVerifyDagBlocks --> processOneBlock

class BlockProcessor{
public:
	typedef std::chrono::steady_clock::time_point time_t;
	BlockProcessor(FullNode &node);
	~BlockProcessor();
	void add (std::shared_ptr<taraxa::DagBlock> sp, time_t time);
	bool full ();
	bool haveBlocks();
	void stop();
	void processBlocks();
	
	ProcessReturn processOneBlock (std::shared_ptr<DagBlock> blk, 
		time_t time = std::chrono::steady_clock::now(), bool validated = false);
	
private:
	void processManyBlocks(std::unique_lock<std::mutex> &lock);
	void batchVerifyDagBlocks (std::unique_lock<std::mutex> &lock);
	bool stopped_ = false;
	bool active_ = false;
	FullNode & node_;
	std::mutex mutex_;
	std::condition_variable condition_;
	std::unordered_set<taraxa::blk_hash_t> block_hashes_;
	std::deque<std::pair<std::shared_ptr<taraxa::DagBlock>, time_t>> dag_blocks_;
	std::deque<std::pair<std::shared_ptr<taraxa::DagBlock>, time_t>> verified_blocks_;

};

}
#endif
