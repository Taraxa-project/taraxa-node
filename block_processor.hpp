/*
 * @Copyright: Taraxa.io 
 * @Author: Chia-Chun Lin 
 * @Date: 2018-12-07 18:04:02 
 * @Last Modified by: Chia-Chun Lin
 * @Last Modified time: 2018-12-10 20:01:52
 */

#ifndef BLOCK_PROCESSOR_HPP
#define BLOCK_PROCESSOR_HPP
#include <iostream>
#include <unordered_set>
#include <mutex>
#include <queue>
#include "state_block.hpp"
#include "util.hpp"

// Process blocks with multithreading
namespace taraxa{

const unsigned int MAX_STATE_BLOCKS = 16384;
class FullNode;

// Call procedure
// processBlocks --> processManyBlocks --> batchVerifyStateBlocks --> processOneBlock

class BlockProcessor{
public:
	typedef std::chrono::steady_clock::time_point time_t;
	BlockProcessor(taraxa::FullNode &node): node_(node){}
	~BlockProcessor();
	void add (std::shared_ptr<taraxa::StateBlock> sp, time_t time);
	bool full ();
	bool haveBlocks();
	void stop();
	void processBlocks();
	
	ProcessReturn processOneBlock (std::shared_ptr<taraxa::StateBlock> blk, 
		time_t time = std::chrono::steady_clock::now(), bool validated = false);
	
private:
	void processManyBlocks(std::unique_lock<std::mutex> &lock);
	void batchVerifyStateBlocks (std::unique_lock<std::mutex> &lock);
	bool stopped_ = false;
	bool active_ = false;
	taraxa::FullNode & node_;
	std::mutex mutex_;
	std::condition_variable condition_;
	std::unordered_set<taraxa::blk_hash_t> block_hashes_;
	std::deque<std::pair<std::shared_ptr<taraxa::StateBlock>, time_t>> state_blocks_;
	std::deque<std::pair<std::shared_ptr<taraxa::StateBlock>, time_t>> verified_blocks_;

};

}
#endif
